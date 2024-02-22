#pragma once

#include <condition_variable>
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
  std::condition_variable_any& cv_;

  ReadGuard( std::shared_lock<std::shared_mutex>&& lock, const T& data, std::condition_variable_any& cv )
    : lock_( std::move( lock ) )
    , data_( data )
    , cv_( cv )
  {}

public:
  const T* operator->() const { return &data_; }

  const T& get() const { return data_; }

  template<class Predicate>
  void wait( Predicate c )
  {
    cv_.wait( lock_, c );
  }
};

template<typename T>
class WriteGuard
{
  friend class SharedMutex<T>;
  std::unique_lock<std::shared_mutex> lock_;
  T& data_;
  std::condition_variable_any& cv_;

  WriteGuard( std::unique_lock<std::shared_mutex>&& lock, T& data, std::condition_variable_any& cv )
    : lock_( std::move( lock ) )
    , data_( data )
    , cv_( cv )
  {}

public:
  T* operator->() const { return &data_; }

  T& get() const { return data_; }

  template<class Predicate>
  void wait( Predicate c )
  {
    cv_.wait( lock_, c );
  }

  ~WriteGuard() { cv_.notify_all(); }
};

template<typename T>
class SharedMutex
{
  std::shared_mutex mutex_ {};
  std::condition_variable_any cv_ {};
  T data_;

public:
  SharedMutex( auto&&... args )
    : data_( std::forward<decltype( args )>( args )... )
  {}

  ReadGuard<T> read()
  {
    std::shared_lock lock( mutex_ );
    return ReadGuard { std::move( lock ), data_, cv_ };
  }

  WriteGuard<T> write()
  {
    std::unique_lock lock( mutex_ );
    return WriteGuard { std::move( lock ), data_, cv_ };
  }
};
