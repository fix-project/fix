#pragma once

#include <atomic>

#include "handle.hh"
#include "operation.hh"

class Task
{
  Handle handle_;
  Operation operation_;

public:
  constexpr Task()
    : handle_()
    , operation_()
  {}

  constexpr Task( const Task& other )
    : handle_( other.handle_ )
    , operation_( other.operation_ )
  {}

  constexpr Task( Handle handle, Operation operation )
    : handle_( handle )
    , operation_( operation )
  {}

  constexpr Handle handle() const { return handle_; }
  constexpr Operation operation() const { return operation_; }

  bool operator==( const Task other ) const
  {
    return handle_ == other.handle() and operation_ == other.operation();
  }

  template<typename H>
  friend H AbslHashValue( H h, const Task& task )
  {
    return H::combine( std::move( h ), task.handle_, task.operation_ );
  }

  static Task Eval( Handle handle ) { return Task( handle, Operation::Eval ); }
  static Task Apply( Handle handle ) { return Task( handle, Operation::Apply ); }
};
