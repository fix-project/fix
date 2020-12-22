#include <iostream>
#include <memory>

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/IR/Module.h>

#include "ccompiler.hh"

using namespace std;
using namespace clang;
using namespace llvm;

string c_to_elf( const string & wasm_name, const string & c_content, const string & h_content ) 
{
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
  const char *Args[] = { ( wasm_name + ".c" ).c_str() };
  CompilerInvocation::CreateFromArgs( compilerInvocation, Args, *diagEngine );

  CodeGenAction *action = new EmitLLVMOnlyAction();
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

  for ( const auto& i : module->getFunctionList() ) 
  {
    printf("%s\n", i.getName().str().c_str() );
  }

  return wasm_name + c_content + h_content + " ";
}
