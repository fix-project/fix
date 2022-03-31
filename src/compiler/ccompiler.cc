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

  // auto diskFile = RealFS->openFileForRead( "/usr/include/string.h" );
  // cout << ( string )( ( *( *diskFile )->getBuffer( "ignore" ) )->getBuffer() ) << endl;

  // Create arguments
  const char* Args[] = { ( wasm_name + ".c" ).c_str(), "-O2", FIXPOINT_C_INCLUDE_PATH };
  CompilerInvocation::CreateFromArgs( compilerInvocation, Args, *diagEngine );

  // Setup mcmodel
  auto& codegenOptions = compilerInstance.getCodeGenOpts();
  // codegenOptions.CodeModel = "small";
  codegenOptions.RelocationModel = llvm::Reloc::Static;

  LLVMContext context;
  CodeGenAction* action = new EmitLLVMOnlyAction( &context );
  auto& targetOptions = compilerInstance.getTargetOpts();
  targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
  compilerInstance.createDiagnostics( diagPrinter.get(), false );

  if ( !compilerInstance.ExecuteAction( *action ) ) {
    cout << "Failed to execute action." << endl;
    cout << diagOS.str() << endl;
  }

  unique_ptr<llvm::Module> module = action->takeModule();
  if ( !module ) {
    cout << "Failed to take module." << endl;
    cout << diagOS.str() << endl;
  }

  // set up llvm target machine
  auto& CodeGenOpts = compilerInstance.getCodeGenOpts();
  auto& Diagnostics = compilerInstance.getDiagnostics();

  // if ( module->getTargetTriple() != targetOptions.Triple )
  //{
  // cout << "Wrong target triple" << endl;
  // module->setTargetTriple( targetOptions.Triple );
  // }

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
