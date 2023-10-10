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
  std::atomic<bool> shutdown_ = false;
  std::atomic<size_t> size_ = 0;

public:
  Channel() {};

  template<class... Ts>
  void emplace( Ts... args )
  {
    data_.emplace_back( args... );
  }

  void push( T&& item )
  {
    if ( shutdown_ ) {
      throw ChannelClosed {};
    }
    data_.enqueue( std::move( item ) );
    ++size_;
    size_.notify_all();
  }

  std::optional<T> pop()
  {
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
    while ( true ) {
      size_.wait( 0 );
      auto result = pop();
      if ( result )
        return *result;
    }
  }

  size_t size_approx() { return data_.size_approx(); }

  void operator<<( T&& item ) { push( std::move( item ) ); }
  void operator>>( std::optional<T>& item ) { item = pop(); }

  void close()
  {
    shutdown_ = true;
    ++size_;
    size_.notify_all();
  }
};
