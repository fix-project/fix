#pragma once

#include <mutex>
#include <shared_mutex>
#include <vector>

template<typename T>
class concurrent_vector
{
private:
  std::vector<T> vector_;
  std::shared_mutex vector_mutex_;

public:
  concurrent_vector()
    : vector_()
    , vector_mutex_()
  {}

  T& at( const typename std::vector<T>::size_type pos )
  {
    std::shared_lock lock( vector_mutex_ );
    return vector_.at( pos );
  }

  typename std::vector<T>::size_type size()
  {
    std::shared_lock lock( vector_mutex_ );
    return vector_.size();
  }

  size_t push_back( T&& value )
  {
    std::unique_lock lock( vector_mutex_ );
    size_t local_id = vector_.size();
    vector_.push_back( std::move( value ) );
    return local_id;
  }

  template<class... Args>
  size_t emplace_back( Args&&... args )
  {
    std::unique_lock lock( vector_mutex_ );
    size_t local_id = vector_.size();
    vector_.emplace_back( args... );
    return local_id;
  }
};
