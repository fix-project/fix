#pragma once

#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>

/** An inter-thread communication data structure, analogous to Go's channels. Semantically, a Channel is a way to
 * transfer ownership of an object from one location to another, so for safety reasons it only
 * accepts rvalue references.
 */
template<typename T>
class Channel
{
  std::deque<T> data_ {};
  std::unique_ptr<std::shared_mutex> mutex_ = std::make_unique<std::shared_mutex>();
  std::condition_variable_any cv_ {};

public:
  Channel() {};

  template<class... Ts>
  void emplace( Ts... args )
  {
    data_.emplace_back( args... );
  }

  void push( T&& item )
  {
    std::unique_lock lock_( *mutex_ );
    data_.push_back( std::move( item ) );
    cv_.notify_all();
  }

  std::optional<T> pop()
  {
    std::unique_lock lock_( *mutex_ );
    if ( data_.empty() )
      return std::nullopt;
    T item = std::move( data_.front() );
    data_.pop_front();
    return item;
  }

  T pop_or_wait()
  {
    std::unique_lock lock_( *mutex_ );
    cv_.wait( lock_, [&] { return not data_.empty(); } );
    T item = std::move( data_.front() );
    data_.pop_front();
    return item;
  }

  bool empty()
  {
    std::shared_lock lock_( *mutex_ );
    return data_.empty();
  }
  size_t size()
  {
    std::shared_lock lock_( *mutex_ );
    return data_.size();
  }

  void operator<<( T&& item ) { push( std::move( item ) ); }
  void operator>>( std::optional<T>& item ) { item = pop(); }
};
