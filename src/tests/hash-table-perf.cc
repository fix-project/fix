#include "hash_table.hh"
#include "runtimestorage.hh"
#include "timer.hh"

#define SIZE 1000000
#define LOAD 10000
#define READLOAD 40000
#define THREADS 1

using namespace std;

struct Identity
{
  template<typename T>
  std::size_t operator()( Handle<T> const& handle ) const noexcept
  {
    u64x4 dwords = (u64x4)handle.content;
    return dwords[0];
  }
};

vector<pair<Handle<Blob>, size_t>> storage;
SharedMutex<absl::flat_hash_map<Handle<Blob>, size_t, Identity>> guarded_absl_table;
absl::flat_hash_map<Handle<Blob>, size_t, Identity> absl_table;

FixTable<Blob, size_t, Identity> fix_table( SIZE );
size_t sum;

int main( void )
{
  guarded_absl_table.write()->reserve( SIZE );
  absl_table.reserve( SIZE );

  for ( size_t i = 0; i < LOAD; i++ ) {
    storage.push_back( { Handle<Named>( rand(), 1024 ), rand() } );
  }

  global_timer().start<Timer::Category::Execution>();
  for ( size_t i = 0; i < LOAD; i++ ) {
    guarded_absl_table.write()->insert( storage.at( i ) );
  }
  global_timer().stop<Timer::Category::Execution>();
  global_timer().average( cout, LOAD );

  reset_global_timer();

  global_timer().start<Timer::Category::Execution>();
  for ( size_t i = 0; i < LOAD; i++ ) {
    fix_table.insert( storage.at( i ).first, storage.at( i ).second );
  }
  global_timer().stop<Timer::Category::Execution>();
  global_timer().average( cout, LOAD );

  reset_global_timer();

  vector<size_t> random_access;
  for ( size_t i = 0; i < READLOAD; i++ ) {
    random_access.push_back( rand() % LOAD );
  }

  global_timer().start<Timer::Category::Execution>();
  for ( size_t i = 0; i < READLOAD; i++ ) {
    sum += guarded_absl_table.read()->at( storage.at( random_access.at( i ) ).first );
  }
  global_timer().stop<Timer::Category::Execution>();
  global_timer().average( cout, READLOAD );

  reset_global_timer();

  global_timer().start<Timer::Category::Execution>();
  for ( size_t i = 0; i < READLOAD; i++ ) {
    sum += fix_table.get( storage.at( random_access.at( i ) ).first ).value();
  }
  global_timer().stop<Timer::Category::Execution>();
  global_timer().average( cout, READLOAD );

  reset_global_timer();

  return 0;
}
