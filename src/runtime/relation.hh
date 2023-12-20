#pragma once

#include "operation.hh"
#include "task.hh"

enum class RelationType : uint8_t
{
  Apply,
  Eval,
  Fill,
  COUNT
};

static constexpr const char* RELATIONTYPE_NAMES[static_cast<uint8_t>( RelationType::COUNT )] = { "Apply", "Eval", "Fill" };

class Relation
{
  RelationType type_ {};
  Handle lhs_;
  Handle rhs_;

public:
  constexpr Relation( Task task, Handle handle )
    : type_( [&]() {
      switch ( task.operation() ) {
        case Operation::Apply:
          return RelationType::Apply;

        case Operation::Eval:
          return RelationType::Eval;

        case Operation::Fill:
          return RelationType::Fill;

        default:
          throw std::runtime_error( "Invalid task for relation" );
      }
    }() )
    , lhs_( task.handle() )
    , rhs_( handle )
  {}

  constexpr RelationType type() const { return type_; }
  constexpr Handle lhs() const { return lhs_; }
  constexpr Handle rhs() const { return rhs_; }

  bool operator==( const Relation other ) const
  {
    return type_ == other.type() and lhs_ == other.lhs() and rhs_ == other.rhs();
  }

  friend std::ostream& operator<<( std::ostream& s, const Relation relation )
  {
    s << relation.lhs() << " --" << RELATIONTYPE_NAMES[static_cast<uint8_t>( relation.type() )] << "--> "
      << relation.rhs();
    return s;
  }
};
