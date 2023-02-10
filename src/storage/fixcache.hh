#pragma once

#include "absl/container/flat_hash_map.h"
#include "entry.hh"
#include "name.hh"
#include <shared_mutex>

class fixcache
{
private:
  absl::flat_hash_map<Name, Entry, NameHash> fixcache_;
  std::shared_mutex fixcache_mutex_;

public:
  fixcache()
    : fixcache_()
    , fixcache_mutex_()
  {}

  Entry& at( Name name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return fixcache_.at( name );
  }

  bool contains( Name name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return fixcache_.contains( name );
  }

  Name get_name( Name name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return fixcache_.at( name ).name;
  }

  std::shared_ptr<std::atomic<int64_t>> get_pending( Name name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return fixcache_.at( name ).pending;
  }

  void insert_or_update( Name name, Name value, int64_t pending )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( fixcache_.contains( name ) ) {
      Entry& entry = fixcache_.at( name );
      entry.name = value;
      ( *entry.pending.get() ) = pending;
      if ( pending == 0 ) {
        ( *entry.pending.get() ).notify_all();
      }
    } else {
      fixcache_.insert_or_assign( name, Entry( value, pending ) );
    }
  }

  void set_name( Name name, Name value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    Entry& entry = fixcache_.at( name );
    entry.name = value;
  }

  void insert_next( Name name, Name value, int64_t pending )
  {
    std::unique_lock lock( fixcache_mutex_ );
    for ( size_t i = 1;; ++i ) {
      name.set_index( i );
      auto ins = fixcache_.insert( { name, Entry( value, pending ) } );

      // If insertion was sucessful
      if ( ins.second ) {
        // std::cerr << "ins next: " << name << "\n";
        break;
      }
    }
  }
};
