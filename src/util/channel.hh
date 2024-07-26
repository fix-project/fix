#pragma once

#include <concurrentqueue/blockingconcurrentqueue.h>
#include <optional>

class ChannelClosed : public std::exception
{
public:
  using std::exception::what;
  const char* what() { return "Channel was closed."; }
};

/**
 * An inter-thread communication data structure, analogous to Go's channels.
 */
template<typename T>
class Channel
{
  moodycamel::BlockingConcurrentQueue<T> data_ {};
  std::atomic<bool> shutdown_ = false;

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
  }

  void move_push( T&& item )
  {
    if ( shutdown_ ) {
      throw ChannelClosed {};
    }
    data_.enqueue( std::move( item ) );
  }

  void push( T item )
  {
    if ( shutdown_ ) {
      throw ChannelClosed {};
    }
    data_.enqueue( std::move( item ) );
  }

  std::optional<T> pop()
  {
    if ( shutdown_ ) {
      throw ChannelClosed();
    }
    T item;
    if ( data_.try_dequeue( item ) ) {
      return item;
    }
    return {};
  }

  T pop_or_wait()
  {
    while ( true ) {
      T item;
      data_.wait_dequeue( item );
      if ( shutdown_ ) {
        data_.enqueue( item );
        throw ChannelClosed();
      }
      return item;
    }
  }

  size_t size_approx() { return data_.size_approx(); }

  void operator<<( T&& item ) { push( std::move( item ) ); }
  void operator<<( T item ) { push( item ); }
  void operator>>( std::optional<T>& item ) { item = pop(); }
  void operator>>( T& item ) { item = pop_or_wait(); }

  void close()
  {
    shutdown_ = true;
    data_.enqueue( T() );
  }

  ~Channel() { close(); }
};
