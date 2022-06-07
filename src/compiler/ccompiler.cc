#include <bitset>
#include <iostream>
#include <memory>

#include <clang/Basic/FileManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/BackendUtil.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
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

string c_to_elf( const string_view c_content,
                 const string_view h_content,
                 const string_view fixpoint_header,
                 const string_view wasm_rt_content )
{
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();

  // Create compiler instance
  clang::CompilerInstance compilerInstance;

  // Create diagnostic engine
  string diagOutput;
  raw_string_ostream diagOS( diagOutput );
  auto diagPrinter = make_unique<TextDiagnosticPrinter>( diagOS, new DiagnosticOptions() );

  IntrusiveRefCntPtr<DiagnosticsEngine> diagEngine
    = CompilerInstance::createDiagnostics( new DiagnosticOptions(), diagPrinter.get(), false );

  // Create File System
  IntrusiveRefCntPtr<vfs::OverlayFileSystem> RealFS( new vfs::OverlayFileSystem( vfs::getRealFileSystem() ) );
  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> InMemFS( new vfs::InMemoryFileSystem() );
  RealFS->pushOverlay( InMemFS );

  InMemFS->addFile( "./function.c", 0, MemoryBuffer::getMemBuffer( c_content ) );
  InMemFS->addFile( "./function.h", 0, MemoryBuffer::getMemBuffer( fixpoint_header ) );
  InMemFS->addFile( "./function_fixpoint.h", 0, MemoryBuffer::getMemBuffer( h_content ) );
  InMemFS->addFile( "./wasm-rt.h", 0, MemoryBuffer::getMemBuffer( wasm_rt_content ) );

  compilerInstance.setFileManager( new FileManager( FileSystemOptions {}, RealFS ) );

  // Create compiler invocation
  const char* Args[] = { "clang", "-c", "-O2", "-Wall", "-Werror", "-march=native", "-mtune=native", "function.c" };
  std::shared_ptr<CompilerInvocation> compilerInvocation = createInvocationFromCommandLine( Args, diagEngine );
  if ( !compilerInvocation ) {
    throw runtime_error( "Failed to create compiler Invocation" );
  }

  LLVMContext context;
  std::unique_ptr<CodeGenAction> action( new EmitLLVMOnlyAction( &context ) );
  auto& targetOptions = compilerInstance.getTargetOpts();
  targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
  compilerInstance.createDiagnostics( diagPrinter.get(), false );
  compilerInstance.setInvocation( compilerInvocation );

  // Setup mcmodel
  auto& codegenOptions = compilerInstance.getCodeGenOpts();
  codegenOptions.CodeModel = "large";
  codegenOptions.RelocationModel = llvm::Reloc::Static;

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
