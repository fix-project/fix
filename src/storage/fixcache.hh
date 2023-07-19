#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "entry.hh"
#include "task.hh"

class fixcache
{
public:
  using QueueFunction = std::function<void( Task )>;

private:
  absl::flat_hash_map<Task, std::optional<Handle>, absl::Hash<Task>> fixcache_;

  absl::flat_hash_map<std::pair<Task, std::size_t>, Task, absl::Hash<std::pair<Task, std::size_t>>>
    dependency_cache_;
  absl::flat_hash_map<Task, std::shared_ptr<std::atomic<std::size_t>>, absl::Hash<Task>> blocked_count_;
  std::shared_mutex fixcache_mutex_;
  std::condition_variable_any fixcache_cv_;

  void insert_dependency( Task dependee, Task depender )
  {
    if ( dependee == depender ) {
      throw std::runtime_error( "attempted to insert self-dependency" );
    }
    for ( size_t i = 1;; ++i ) {
      // we deal with duplicates by adding a dependency index; we linear scan until we find a free index
      auto pair = std::make_pair( dependee, i );
      auto ins = dependency_cache_.insert( { pair, depender } );
      // If insertion was sucessful
      if ( ins.second ) {
        break;
      }
    }
  }

  void unblock_jobs( Task finished, QueueFunction queue )
  {
    for ( int i = 1;; ++i ) {
      auto pair = std::make_pair( finished, i );
      if ( dependency_cache_.contains( pair ) ) {
        Task requestor = dependency_cache_.at( pair );
        size_t blocked = blocked_count_.at( requestor )->fetch_sub( 1 ) - 1;
        if ( blocked == 0 ) {
          queue( requestor );
        }
      } else {
        break;
      }
    }
  }

  bool add_task( Task task, QueueFunction queue )
  {
    if ( not fixcache_.contains( task ) ) {
      fixcache_.insert( { task, std::nullopt } );
      blocked_count_.insert( { task, std::make_shared<std::atomic<std::size_t>>( 0 ) } );
      queue( task );
      return true;
    }
    return false;
  }

public:
  fixcache()
    : fixcache_()
    , dependency_cache_()
    , blocked_count_()
    , fixcache_mutex_()
    , fixcache_cv_()
  {}

  std::optional<Handle> get( Task task )
  {
    std::shared_lock lock( fixcache_mutex_ );
    if ( fixcache_.contains( task ) ) {
      return fixcache_.at( task );
    } else {
      return {};
    }
  }

  void cache( Task task, Handle result, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( fixcache_.at( task ).has_value() ) {
      throw std::runtime_error( "double-cache" );
    }
    if ( blocked_count_.at( task )->load() != 0 ) {
      throw std::runtime_error( "caching result of task which is still blocked" );
    }
    fixcache_.at( task ) = result;
    fixcache_cv_.notify_all();
    unblock_jobs( task, queue );
  }

  void start( Task task, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    add_task( task, queue );
  }

  std::optional<Handle> get_or_add_dependency( Task dependee, Task depender, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    add_task( dependee, queue );
    if ( fixcache_.at( dependee ).has_value() ) {
      return fixcache_.at( dependee );
    }
    insert_dependency( dependee, depender );
    blocked_count_.at( depender )->fetch_add( 1 );
    return {};
  }

  void increment_blocking_count( Task task, std::size_t count = 1 )
  {
    std::shared_lock lock( fixcache_mutex_ );
    blocked_count_.at( task )->fetch_add( count );
  }

  size_t add_dependency_or_decrement_blocking_count( Task dependee, Task depender, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    add_task( dependee, queue );
    if ( fixcache_.at( dependee ).has_value() ) {
      return blocked_count_.at( depender )->fetch_sub( 1 ) - 1;
    }
    insert_dependency( dependee, depender );
    return *blocked_count_.at( depender );
  }

  Handle get_blocking( Task target )
  {
    using namespace std;
    unique_lock lock( fixcache_mutex_ );
    fixcache_cv_.wait(
      lock, [this, target] { return fixcache_.contains( target ) and fixcache_.at( target ).has_value(); } );
    return fixcache_.at( target ).value();
  }
};
