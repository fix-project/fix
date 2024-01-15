#pragma once

#include <mutex>
#include <shared_mutex>

template<typename T>
class SharedMutex;

template<typename T>
class ReadGuard
{
  friend class SharedMutex<T>;
  std::shared_lock<std::shared_mutex> lock_;
  const T& data_;

  const T& operator->() { return data_; }
};

template<typename T>
class WriteGuard
{
  friend class SharedMutex<T>;
  std::unique_lock<std::shared_mutex> lock_;
  T& data_;

  T& operator->() { return data_; }
};

template<typename T>
class SharedMutex
{
  std::shared_mutex mutex_;
  T data_;

public:
  template<typename... Args>
  SharedMutex( Args&&... args )
    : data_( std::forward<Args...>( args... ) )
  {}

  ReadGuard<T> read()
  {
    std::shared_lock lock( mutex_ );
    return ReadGuard { lock, data_ };
  }

  WriteGuard<T> write()
  {
    std::unique_lock lock( mutex_ );
    return WriteGuard { lock, data_ };
  }
};
