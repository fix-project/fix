#include "hash_map.hh"
#include "name.hh"
#include "storage.hh"

using namespace std;

#define SIZE 4096
Table<AbslHash> new_map;
AbslHash hash_name;

int main()
{
  vector<Name> names;
  for ( size_t i = 0; i < SIZE; i++ ) {
    names.push_back( Name( i, 4, ContentType::Blob ) );
  }

  new_map.reserve( 13 );
  for ( size_t i = 0; i < SIZE; i++ ) {
    new_map.put_ptr( names.at( i ), i );
  }

  for ( size_t i = 0; i < SIZE; i++ ) {
    auto value = new_map.get_ptr( names.at( i ) );
    if ( value != i ) {
      cout << "Different value " << value << " " << i << endl;
    }
  }

  return 0;
}
