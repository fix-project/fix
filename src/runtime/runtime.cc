#include "runtime.hh"
#include "base16.hh"
#include "relation.hh"
#include "util.hh"
#include <filesystem>
#include <functional>
#include <stdexcept>

using namespace std;

void Runtime::visit( Relation root, std::function<void( Relation )> visitor )
{
  switch ( root.type() ) {
    case RelationType::Apply:
      VLOG( 1 ) << "Visiting apply";
      break;

    case RelationType::TracedBy:
    case RelationType::COUNT:
      throw std::runtime_error( "Visiting not-visitable relation" );
      break;

    case RelationType::Eval: {
      VLOG( 1 ) << "Visiting eval";
      auto name = root.lhs();

      switch ( name.get_content_type() ) {
        case ContentType::Blob:
          break;

        case ContentType::Tree:
        case ContentType::Tag: {
          for ( const auto entry : storage_.get_tree( name.as_tree() ) ) {
            if ( auto relation = get_relation( Task::Eval( entry ) ); relation.has_value() ) {
              visit( relation.value(), visitor );
            }
          }
          break;
        }

        case ContentType::Thunk: {
          Handle encode = name.as_tree();

          auto relation = get_relation( Task::Eval( encode ) );
          if ( !relation.has_value() )
            return;
          visit( relation.value(), visitor );

          relation = get_relation( Task::Apply( relation->rhs() ) );
          if ( !relation.has_value() )
            return;
          visit( relation.value(), visitor );

          relation = get_relation(
            Task::Eval( name.is_shallow() ? Handle::make_shallow( relation->rhs() ) : relation->rhs() ) );
          if ( !relation.has_value() )
            return;
          visit( relation.value(), visitor );
          break;
        }
      }
    }
  }
  VLOG( 1 ) << "Calling visitor " << root;
  visitor( root );
}

void Runtime::shallow_visit( Relation root, std::function<void( Relation )> visitor )
{
  switch ( root.type() ) {
    case RelationType::Apply:
      VLOG( 1 ) << "Visiting apply";
      break;

    case RelationType::TracedBy:
    case RelationType::COUNT:
      throw std::runtime_error( "Visiting not-visitable relation" );
      break;

    case RelationType::Eval: {
      VLOG( 1 ) << "Visiting eval";
      auto name = root.lhs();

      switch ( name.get_content_type() ) {
        case ContentType::Blob:
          break;

        case ContentType::Tree:
        case ContentType::Tag: {
          for ( const auto entry : storage_.get_tree( name.as_tree() ) ) {
            if ( auto relation = get_relation( Task::Eval( entry ) ); relation.has_value() ) {
              visitor( relation.value() );
            }
          }
          break;
        }

        case ContentType::Thunk: {
          Handle encode = name.as_tree();

          auto relation = get_relation( Task::Eval( encode ) );
          if ( !relation.has_value() )
            return;
          visitor( relation.value() );

          relation = get_relation( Task::Apply( relation->rhs() ) );
          if ( !relation.has_value() )
            return;
          visitor( relation.value() );

          relation = get_relation(
            Task::Eval( name.is_shallow() ? Handle::make_shallow( relation->rhs() ) : relation->rhs() ) );
          if ( !relation.has_value() )
            return;
          visitor( relation.value() );
          break;
        }
      }
    }
  }
}

void Runtime::serialize( Task task )
{
  if ( auto relation = cache_.get_relation( task ); relation.has_value() ) {
    VLOG( 1 ) << "Visiting relations" << endl;
    visit( relation.value(), [this]( Relation relation ) {
      this->storage().serialize_relation( relation );
      this->serialize( relation.lhs() );
      this->serialize( relation.rhs() );
    } );
  }
}

void Runtime::serialize( Handle name )
{
  storage_.visit( name, [this]( Handle handle ) {
    this->storage().serialize_object( handle, this->storage().get_fix_repo() / "objects" );
    auto trace_relations = cache_.get_relation( handle );
    for ( const auto& relation : trace_relations ) {
      this->storage().serialize_relation( relation );
      storage_.visit( relation.rhs(), [this]( Handle handle ) {
        this->storage().serialize_object( handle, this->storage().get_fix_repo() / "objects" );
      } );
    }
  } );
}

void Runtime::deserialize_relations()
{
  auto dir = storage_.get_fix_repo() / "relations";
  if ( not filesystem::exists( dir ) ) {
    return;
  }

  for ( const auto& file :
        filesystem::directory_iterator( dir / RELATIONTYPE_NAMES[to_underlying( RelationType::Apply )] ) ) {
    Handle name( base16::decode( file.path().filename().string() ) );

    auto tmp = OwnedTree::from_file( file );
    cache_.finish( Task::Apply( name ), tmp.at( 0 ) );
  }

  for ( const auto& file :
        filesystem::directory_iterator( dir / RELATIONTYPE_NAMES[to_underlying( RelationType::Eval )] ) ) {
    Handle name( base16::decode( file.path().filename().string() ) );

    auto tmp = OwnedTree::from_file( file );
    cache_.finish( Task::Eval( name ), tmp.at( 0 ) );
  }

  for ( const auto& file :
        filesystem::directory_iterator( dir / RELATIONTYPE_NAMES[to_underlying( RelationType::TracedBy )] ) ) {
    Handle name( base16::decode( file.path().filename().string() ) );

    auto tmp = OwnedTree::from_file( file );
    for ( const auto& entry : tmp.span() ) {
      cache_.trace( name, entry );
    }
  }

  return;
}
