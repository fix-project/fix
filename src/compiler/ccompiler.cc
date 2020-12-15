#include <memory>

#include <clang/Frontend/CompilerInstance.h>

#include "ccompiler.hh"

using namespace std;

string c_to_elf( const string & c_name, const string & c_content ) 
{
  llvm::StringRef str_ref( c_content );
  llvm::MemoryBufferRef buf_ref( { c_content }, { c_name } ); 

  // Create diagnostic engine
  clang::DiagnosticOptions diagOptions;
  clang::DiagnosticIDs diagIDs;
  clang::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOptPtr( &diagOptions );
  clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDPtr( &diagIDs );
  clang::DiagnosticsEngine diagEngine( diagIDPtr, diagOptPtr );

  // Create compiler instance
  clang::CompilerInstance compilerInstance;
  auto compilerInvocation = compilerInstance.getInvocation();

  // Create arguments



  return c_content + " ";
}
