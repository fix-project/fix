#pragma once

#include "handle.hh"
#include "operation.hh"

/**
 * A "Task" is the base unit of work in Fix.  It corresponds to the execution of a certain ::Operation on a certain
 * Handle.  Tasks are immutable and trivially constructable/destructable; they are meant to be used only as keys for
 * FixCache and RuntimeWorker.
 */
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

  friend std::ostream& operator<<( std::ostream& s, const Task task )
  {
    switch ( task.operation() ) {
      case Operation::Apply:
        s << "Apply";
        break;
      case Operation::Eval:
        s << "Eval";
        break;
      case Operation::Fill:
        s << "Fill";
        break;
    }
    s << "( " << task.handle_ << " )";
    return s;
  }

  template<typename H>
  friend H AbslHashValue( H h, const Task& task )
  {
    return H::combine( std::move( h ), task.handle_, task.operation_ );
  }

  static Task Eval( Handle handle ) { return Task( handle, Operation::Eval ); }
  static Task Apply( Handle handle ) { return Task( handle, Operation::Apply ); }
  static Task Fill( Handle handle ) { return Task( handle, Operation::Fill ); }

  Task& operator=( const Task& other )
  {
    handle_ = other.handle_;
    operation_ = other.operation_;
    return *this;
  }
};

namespace std {
template<>
struct hash<Task>
{
  size_t operator()( const Task& x ) const
  {
    return hash<Handle>()( x.handle() ) ^ hash<Operation>()( x.operation() );
  }
};
}
