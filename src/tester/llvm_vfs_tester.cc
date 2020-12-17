#include <iostream>
#include <string>

#include <llvm/Support/VirtualFileSystem.h>

using namespace std;
using namespace llvm;

int main()
{
  string file_content ("hello123456");

  vfs::InMemoryFileSystem FS;
  FS.addFile( "/hello", 0, MemoryBuffer::getMemBuffer( file_content ) );
  auto File = FS.openFileForRead("/hello");

  cout << ( string )( ( *( *File )->getBuffer( "hello" ) )->getBuffer() ) << endl;

  return 0;
}
