#include <bitset>
#include <iostream>
#include <memory>

#include <clang/Basic/FileManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/BackendUtil.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/VirtualFileSystem.h>

#include "ccompiler.hh"
#include "include-path.hh"

using namespace std;
using namespace clang;
using namespace llvm;

string c_to_elf( const string& wasm_name,
                 const string& c_content,
                 const string& h_content,
                 const string& fixpoint_header,
                 const string& wasm_rt_content )
{
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Create compiler instance
  clang::CompilerInstance compilerInstance;
  auto& compilerInvocation = compilerInstance.getInvocation();

  // Create diagnostic engine
  auto& diagOpt = compilerInvocation.getDiagnosticOpts();
  diagOpt.DiagnosticLogFile = "-";

  string diagOutput;
  raw_string_ostream diagOS( diagOutput );
  auto diagPrinter = make_unique<TextDiagnosticPrinter>( diagOS, new DiagnosticOptions() );

  IntrusiveRefCntPtr<DiagnosticsEngine> diagEngine
    = compilerInstance.createDiagnostics( &diagOpt, diagPrinter.get(), false );

  // Create File System
  IntrusiveRefCntPtr<vfs::OverlayFileSystem> RealFS( new vfs::OverlayFileSystem( vfs::getRealFileSystem() ) );
  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> InMemFS( new vfs::InMemoryFileSystem() );
  RealFS->pushOverlay( InMemFS );

  InMemFS->addFile( "./" + wasm_name + ".c", 0, MemoryBuffer::getMemBuffer( c_content ) );
  InMemFS->addFile( "./" + wasm_name + ".h", 0, MemoryBuffer::getMemBuffer( fixpoint_header ) );
  InMemFS->addFile( "./" + wasm_name + "_fixpoint.h", 0, MemoryBuffer::getMemBuffer( h_content ) );
  InMemFS->addFile( "./wasm-rt.h", 0, MemoryBuffer::getMemBuffer( wasm_rt_content ) );

  compilerInstance.setFileManager( new FileManager( FileSystemOptions {}, RealFS ) );

  // Create arguments
  string wasm_file_name = wasm_name + ".c";
  const char* Args[] = { wasm_file_name.c_str(), "-O2", FIXPOINT_C_INCLUDE_PATH };
  CompilerInvocation::CreateFromArgs( compilerInvocation, Args, *diagEngine );

  // Setup mcmodel
  auto& codegenOptions = compilerInstance.getCodeGenOpts();
  codegenOptions.RelocationModel = llvm::Reloc::Static;

  LLVMContext context;
  CodeGenAction* action = new EmitLLVMOnlyAction( &context );
  auto& targetOptions = compilerInstance.getTargetOpts();
  targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
  compilerInstance.createDiagnostics( diagPrinter.get(), false );

  // add host features
  llvm::StringMap<bool> FeatureMap;
  if ( llvm::sys::getHostCPUFeatures( FeatureMap ) ) {
    targetOptions.FeatureMap = FeatureMap;
  } else {
    throw runtime_error( "Failed to read host feature" );
  }
  targetOptions.FeatureMap = FeatureMap;

  if ( !compilerInstance.createTarget() ) {
    cout << diagOS.str() << endl;
    throw runtime_error( "Failed to create target" );
  }

  if ( !compilerInstance.ExecuteAction( *action ) ) {
    cout << diagOS.str() << endl;
    throw runtime_error( "Failed to emit llvm" );
  }

  unique_ptr<llvm::Module> module = action->takeModule();
  if ( !module ) {
    cout << diagOS.str() << endl;
    throw runtime_error( "Failed to take module" );
  }

  // set up codegenopts
  auto& CodeGenOpts = compilerInstance.getCodeGenOpts();
  auto& Diagnostics = compilerInstance.getDiagnostics();

  std::string res;
  raw_string_ostream Str_OS( res );
  auto OS = make_unique<buffer_ostream>( Str_OS );

  EmitBackendOutput( Diagnostics,
                     compilerInstance.getHeaderSearchOpts(),
                     CodeGenOpts,
                     compilerInstance.getTargetOpts(),
                     compilerInstance.getLangOpts(),
                     compilerInstance.getTarget().getDataLayoutString(),
                     module.get(),
                     Backend_EmitObj,
                     std::move( OS ) );

  return res;
}
