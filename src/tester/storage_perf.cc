#include "hash_map.hh"
#include "name.hh"
#include "object.hh"
#include "storage.hh"
#include "timer.hh"

#define SIZE 1000000

using namespace std;

struct IdHash
{
  size_t operator()( size_t const& id ) const noexcept { return id; }
};

int sum = 0;
absl::Hash<uint64_t> hash_64;
AbslHash hash_name;
vector<uint64_t> storage;
absl::flat_hash_map<size_t, uint64_t> raw_hash;
absl::flat_hash_map<Name, uint64_t> absl_hash;
absl::flat_hash_map<Name, uint64_t> absl_hash_reserved;

absl::flat_hash_map<Name, Blob> absl_blob;
Table<AbslHash> new_map;

int main()
{
  // Compare the performance of: 1) vector 2) hash table with identity function 3) hash table with absl hash
  {
    cout << "vector storage..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      storage.push_back( i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      sum += storage.at( i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  vector<uint64_t> integers;
  for ( size_t i = 0; i < SIZE * 4; i++ ) {
    integers.push_back( rand() % SIZE );
  }

  {
    cout << "absl hash uint64_t..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE * 4; i += 4 ) {
      raw_hash.try_emplace( integers.at( i ), i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE * 4; i += 4 ) {
      sum += raw_hash.at( integers.at( i ) );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  vector<Name> names;
  for ( size_t i = 0; i < SIZE; i++ ) {
    names.push_back( Name( i, 4, ContentType::Blob ) );
  }

  {
    cout << "name absl hash..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      absl_hash.try_emplace( names.at( i ), i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      sum += absl_hash.at( names.at( i ) );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  absl_hash_reserved.reserve( SIZE );
  {
    cout << "reserved name absl hash..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      absl_hash_reserved.try_emplace( names.at( i ), i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      sum += absl_hash_reserved.at( names.at( i ) );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  absl_blob.reserve( SIZE * 2 );
  new_map.reserve( 21 );
  {
    cout << "absl blob..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      absl_blob.try_emplace( names.at( i ), make_blob( 0 ) );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      sum += absl_blob.at( names.at( i ) ).size();
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    cout << "new map blob..." << endl;
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      new_map.put_ptr( names.at( i ), i );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  {
    global_timer().start<Timer::Category::Execution>();
    for ( size_t i = 0; i < SIZE; i++ ) {
      sum += new_map.get_ptr( names.at( i ) );
    }
    global_timer().stop<Timer::Category::Execution>();
    global_timer().average( cout, SIZE );
    reset_global_timer();
  }

  cout << sizeof( mPair ) << endl;
  cout << sum << endl;
  return 0;
}
