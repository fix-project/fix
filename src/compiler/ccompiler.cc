#include <bitset>
#include <iostream>
#include <memory>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/CodeGen/BackendUtil.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/TargetSelect.h>

#include "ccompiler.hh"

using namespace std;
using namespace clang;
using namespace llvm;

string c_to_elf( const string & wasm_name, const string & c_content, const string & h_content ) 
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
  raw_string_ostream diagOS ( diagOutput );
  auto diagPrinter = make_unique<TextDiagnosticPrinter>( diagOS, new DiagnosticOptions() );

  IntrusiveRefCntPtr<DiagnosticsEngine> diagEngine = compilerInstance.createDiagnostics( &diagOpt, diagPrinter.get(), false );

  // Create File System
  IntrusiveRefCntPtr<vfs::OverlayFileSystem> RealFS( new vfs::OverlayFileSystem( vfs::getRealFileSystem() ) );
  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> InMemFS( new vfs::InMemoryFileSystem() );
  RealFS->pushOverlay( InMemFS );

  InMemFS->addFile( "./" + wasm_name + ".c", 0, MemoryBuffer::getMemBuffer( c_content ) );
  InMemFS->addFile( "./" + wasm_name + ".h", 0, MemoryBuffer::getMemBuffer( h_content ) );

  compilerInstance.setFileManager( new FileManager( FileSystemOptions{}, RealFS ) );

  // Create arguments
  const char *Args[] = { ( wasm_name + ".c" ).c_str(), "-O2" };
  CompilerInvocation::CreateFromArgs( compilerInvocation, Args, *diagEngine );

  LLVMContext context;
  CodeGenAction *action = new EmitLLVMOnlyAction( &context );
  auto &targetOptions = compilerInstance.getTargetOpts();
  targetOptions.Triple = llvm::sys::getDefaultTargetTriple();
  compilerInstance.createDiagnostics( diagPrinter.get(), false );
  
  if ( !compilerInstance.ExecuteAction( *action ) )
  {
    cout << "Failed to execute action." << endl;
  }

  unique_ptr<llvm::Module> module = action->takeModule() ;
  if ( !module )
  {
    cout << "Failed to take module." << endl;
  }

  // set up llvm target machine
  auto &CodeGenOpts = compilerInstance.getCodeGenOpts();
  auto &Diagnostics = compilerInstance.getDiagnostics();

  if ( module->getTargetTriple() != targetOptions.Triple ) 
  {
    cout << "Wrong target triple" << endl;
    module->setTargetTriple( targetOptions.Triple );
  }

  std::string res;
  raw_string_ostream Str_OS ( res );
  auto OS = make_unique<buffer_ostream>( Str_OS );

  EmitBackendOutput(Diagnostics, compilerInstance.getHeaderSearchOpts(), CodeGenOpts,
                    targetOptions, compilerInstance.getLangOpts(),
                    compilerInstance.getTarget().getDataLayout(), module.get(), Backend_EmitObj,
                    std::move( OS ));
  
  cout << diagOutput << endl; 

  return res;
}
