#include <iostream>
#include <string>

#include <llvm/Support/VirtualFileSystem.h>

using namespace std;
using namespace llvm;

int main( int argc, char* argv[] )
{
  string file_content( "hello123456" );

  IntrusiveRefCntPtr<vfs::OverlayFileSystem> Base( new vfs::OverlayFileSystem( vfs::getRealFileSystem() ) );
  IntrusiveRefCntPtr<vfs::InMemoryFileSystem> FS( new vfs::InMemoryFileSystem() );
  FS->addFile( "/hello", 0, MemoryBuffer::getMemBuffer( file_content ) );
  auto File = FS->openFileForRead( "/hello" );

  cout << (string)( ( *( *File )->getBuffer( "hello" ) )->getBuffer() ) << endl;

  Base->pushOverlay( FS );
  File = Base->openFileForRead( "/hello" );
  cout << (string)( ( *( *File )->getBuffer( "hello" ) )->getBuffer() ) << endl;

  if ( argc >= 2 ) {
    cout << argv[1] << endl;
    auto diskFile = Base->openFileForRead( argv[1] );
    cout << (string)( ( *( *diskFile )->getBuffer( "ignore" ) )->getBuffer() ) << endl;
  }

  return 0;
}
