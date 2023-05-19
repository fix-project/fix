#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>

#include "absl/container/flat_hash_map.h"
#include "entry.hh"

// -2: user thread waiting for the pending
// -1: job done, entry.name is the result Value
// 0: job done, hasn't updated entry.name or call child
// positive number: the job has been started

// For multimap entries
// pending == 0: progress with desired entry name (The desired entry is equivalent to the requestor)
// pending == 1: progress with the entry itself's name (the desired entry needs to be done before the requestor
// starts)
//
#define COMPLETE INT64_C( -1 )
#define PENDING INT64_C( -2 )
#define RUNNING INT64_C( 1 )

class fixcache
{
private:
  absl::flat_hash_map<Name, Entry, NameHash> fixcache_;
  std::shared_mutex fixcache_mutex_;

  void insert_dependency( Name name, Name value, bool start_after )
  {
    for ( size_t i = 1;; ++i ) {
      name.set_index( i );
      auto ins = fixcache_.insert( { name, Entry( value, start_after ) } );

      // If insertion was sucessful
      if ( ins.second ) {
        break;
      }
    }
  }

  bool is_complete( Name name )
  {
    return fixcache_.contains( name ) && fixcache_.at( name ).pending->load() == COMPLETE;
  }

  bool is_pending( Name name )
  {
    return fixcache_.contains( name ) && fixcache_.at( name ).pending->load() == PENDING;
  }

  bool is_running( Name name )
  {
    return fixcache_.contains( name ) && fixcache_.at( name ).pending->load() == RUNNING;
  }

  bool start_job( Name name )
  {
    if ( fixcache_.contains( name ) ) {
      int64_t job_pending = PENDING;
      return fixcache_.at( name ).pending->compare_exchange_strong( job_pending, RUNNING );
    } else {
      fixcache_.insert_or_assign( name, Entry( name, RUNNING ) );
      return true;
    }
  }

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

  void insert_or_assign( Name name, Name value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    fixcache_.insert_or_assign( name, Entry( value ) );
  }

  void set_name( Name name, Name value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    Entry& entry = fixcache_.at( name );
    entry.name = value;
  }

  bool start_after( Name name, Name requestor )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return false;
    }

    insert_dependency( name, requestor, true );
    return true;
  }

  std::optional<Name> continue_after( Name name, Name requestor )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return fixcache_.at( name ).name;
    }

    if ( name != requestor ) {
      insert_dependency( name, requestor, false );
    }
    return {};
  }

  int try_run( Name name, Name requestor )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return -1;
    }

    if ( name != requestor ) {
      insert_dependency( name, requestor, false );
    }

    if ( start_job( name ) ) {
      return 1;
    }

    return 0;
  }

  void pending_start( Name name, Name arg, int64_t pending )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( !is_complete( name ) ) {
      fixcache_.insert_or_assign( name, Entry( arg, pending ) );
    }
  }

  bool try_wait( Name name )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return false;
    }

    if ( !fixcache_.contains( name ) ) {
      fixcache_.insert_or_assign( name, Entry( name, PENDING ) );
    }

    return true;
  }

  bool change_status_to_completed( Name name, Name value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    bool result = false;
    if ( fixcache_.contains( name ) ) {
      Entry& entry = fixcache_.at( name );
      entry.name = value;

      int64_t job_running = RUNNING;
      result = entry.pending->compare_exchange_strong( job_running, COMPLETE );

      ( *entry.pending.get() ).notify_all();
    } else {
      fixcache_.insert_or_assign( name, Entry( value, COMPLETE ) );
      result = true;
    }
    return result;
  }

  std::queue<std::pair<Name, Name>> update_pending_jobs( Name name )
  {
    Name dependency = name;
    std::queue<std::pair<Name, Name>> result;
    for ( int i = 1;; ++i ) {
      dependency.set_index( i );
      if ( contains( dependency ) ) {
        Name requestor = get_name( dependency );
        if ( get_pending( dependency )->load() == 1 ) {
          // Start after
          std::shared_ptr<std::atomic<int64_t>> pending = get_pending( requestor );
          auto sub_result = pending->fetch_sub( 1 ) - 1;

          if ( sub_result == 0 ) {
            result.push( { get_name( requestor ), requestor } );
          }
        } else {
          // Continue after
          result.push( { get_name( name ), requestor } );
        }
      } else {
        break;
      }
    }

    return result;
  }

  std::queue<std::pair<Name, Name>> complete( Name name, Name value )
  {
    if ( change_status_to_completed( name, value ) ) {
      return update_pending_jobs( name );
    } else {
      return {};
    }
  }
};
