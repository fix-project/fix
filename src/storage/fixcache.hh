#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>

#include "absl/container/flat_hash_map.h"
#include "entry.hh"
#include "key.hh"

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
  absl::flat_hash_map<Key, Entry, absl::Hash<Key>> fixcache_;
  std::shared_mutex fixcache_mutex_;

  void insert_dependency( Handle name, Handle value, bool start_after )
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

  bool is_complete( Handle name )
  {
    return fixcache_.contains( name ) && unchecked_at( name ).pending->load() == COMPLETE;
  }

  bool is_pending( Handle name )
  {
    return fixcache_.contains( name ) && unchecked_at( name ).pending->load() == PENDING;
  }

  bool is_running( Handle name )
  {
    return fixcache_.contains( name ) && unchecked_at( name ).pending->load() == RUNNING;
  }

  bool start_job( Handle name )
  {
    if ( fixcache_.contains( name ) ) {
      int64_t job_pending = PENDING;
      return unchecked_at( name ).pending->compare_exchange_strong( job_pending, RUNNING );
    } else {
      fixcache_.insert_or_assign( name, Entry( name, RUNNING ) );
      return true;
    }
  }

  Entry& unchecked_at( Handle name )
  {
    try {
      return fixcache_.at( name );
    } catch ( const std::out_of_range& e ) {
      std::stringstream ss;
      ss << "fixcache does not contain " << name;
      throw std::out_of_range( ss.str() );
    }
  }

public:
  fixcache()
    : fixcache_()
    , fixcache_mutex_()
  {}

  Entry& at( Handle name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return unchecked_at( name );
  }

  bool contains( Handle name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return fixcache_.contains( name );
  }

  Handle get_name( Handle name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return unchecked_at( name ).name;
  }

  std::shared_ptr<std::atomic<int64_t>> get_pending( Handle name )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return unchecked_at( name ).pending;
  }

  void insert_or_assign( Handle name, Handle value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    fixcache_.insert_or_assign( name, Entry( value ) );
  }

  void set_name( Handle name, Handle value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    Entry& entry = unchecked_at( name );
    entry.name = value;
  }

  bool start_after( Handle name, Handle requestor )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return false;
    }

    insert_dependency( name, requestor, true );
    return true;
  }

  std::optional<Handle> continue_after( Handle name, Handle requestor )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( is_complete( name ) ) {
      return unchecked_at( name ).name;
    }

    if ( name != requestor ) {
      insert_dependency( name, requestor, false );
    }
    return {};
  }

  int try_run( Handle name, Handle requestor )
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

  void pending_start( Handle name, Handle arg, int64_t pending )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( !is_complete( name ) ) {
      fixcache_.insert_or_assign( name, Entry( arg, pending ) );
    }
  }

  bool try_wait( Handle name )
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

  bool change_status_to_completed( Handle name, Handle value )
  {
    std::unique_lock lock( fixcache_mutex_ );
    bool result = false;
    if ( fixcache_.contains( name ) ) {
      Entry& entry = unchecked_at( name );
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

  std::queue<std::pair<Handle, Handle>> update_pending_jobs( Handle name )
  {
    Handle dependency = name;
    std::queue<std::pair<Handle, Handle>> result;
    for ( int i = 1;; ++i ) {
      dependency.set_index( i );
      if ( contains( dependency ) ) {
        Handle requestor = get_name( dependency );
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

  std::queue<std::pair<Handle, Handle>> complete( Handle name, Handle value )
  {
    if ( change_status_to_completed( name, value ) ) {
      return update_pending_jobs( name );
    } else {
      return {};
    }
  }
};
