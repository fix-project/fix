#pragma once

#include <concurrentqueue/concurrentqueue.h>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>

class ChannelClosed : public std::exception
{
public:
  using std::exception::what;
  const char* what() { return "Channel was closed."; }
};

/** An inter-thread communication data structure, analogous to Go's channels. Semantically, a Channel is a way to
 * transfer ownership of an object from one location to another, so for safety reasons it only
 * accepts rvalue references.
 */
template<typename T>
class Channel
{
  moodycamel::ConcurrentQueue<T> data_ {};
  std::mutex mutex_ {};
  std::condition_variable cv_ {};
  bool shutdown_ = false;
  size_t size_ = 0;

public:
  Channel() {};

  template<class... Ts>
  void emplace( Ts... args )
  {
    data_.emplace_back( args... );
  }

  void push( T&& item )
  {
    std::unique_lock lock( mutex_ );
    if ( shutdown_ ) {
      throw ChannelClosed {};
    }
    data_.enqueue( std::move( item ) );
    ++size_;
    cv_.notify_one();
  }

  std::optional<T> pop()
  {
    std::unique_lock lock( mutex_ );
    if ( shutdown_ ) {
      throw ChannelClosed();
    }
    T item;
    if ( data_.try_dequeue( item ) ) {
      --size_;
      return item;
    }
    return {};
  }

  T pop_or_wait()
  {
    std::unique_lock lock( mutex_ );
    while ( true ) {
      cv_.wait( lock, [&] { return size_ != 0; } );
      if ( shutdown_ ) {
        throw ChannelClosed();
      }
      T item;
      if ( data_.try_dequeue( item ) ) {
        --size_;
        return item;
      }
    }
  }

  size_t size_approx() { return data_.size_approx(); }

  void operator<<( T&& item ) { push( std::move( item ) ); }
  void operator>>( std::optional<T>& item ) { item = pop(); }

  void close()
  {
    std::unique_lock lock( mutex_ );
    shutdown_ = true;
    ++size_;
    cv_.notify_all();
  }
};
