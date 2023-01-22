#pragma once

#include <queue>

template<typename T>
class concurrent_queue
{
private:
  std::queue<T> queue_;
  std::mutex queue_mutex_;

public:
  concurrent_queue()
    : queue_()
    , queue_mutex_()
  {}

  void push( const T& value )
  {
    std::lock_guard<std::mutex> lock( queue_mutex_ );
    queue_.push( std::move( value ) );
  }

  bool front_pop( T& value )
  {
    std::lock_guard<std::mutex> lock( queue_mutex_ );
    if ( queue_.empty() )
      return false;
    value = queue_.front();
    queue_.pop();
    return true;
  }

  bool empty()
  {
    std::lock_guard<std::mutex> lock( queue_mutex_ );
    return queue_.empty();
  }
};
