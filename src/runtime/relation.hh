#pragma once

#include "task.hh"

class Relation
{
  Task task_;
  Handle handle_;

public:
  constexpr Relation( Task task, Handle handle )
    : task_( task )
    , handle_( handle )
  {}

  constexpr Task task() const { return task_; }
  constexpr Handle handle() const { return handle_; }

  bool operator==( const Relation other ) const { return task_ == other.task() and handle_ == other.handle(); }

  friend std::ostream& operator<<( std::ostream& s, const Relation relation )
  {
    s << relation.task() << " --> " << relation.handle();
    return s;
  }
};
