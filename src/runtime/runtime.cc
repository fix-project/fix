#include "runtime.hh"
#include <functional>

using namespace std;

void Runtime::visit( Relation root, std::function<void( Relation )> visitor )
{
  switch ( root.task().operation() ) {
    case Operation::Apply:
    case Operation::Fill:
      VLOG( 1 ) << "Visiting apply/fill";
      break;

    case Operation::Eval: {
      VLOG( 1 ) << "Visiting eval";
      auto name = root.task().handle();

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

          relation = get_relation( Task::Apply( relation->handle() ) );
          if ( !relation.has_value() )
            return;
          visit( relation.value(), visitor );

          relation = get_relation(
            Task::Eval( name.is_shallow() ? Handle::make_shallow( relation->handle() ) : relation->handle() ) );
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

void Runtime::serialize( Task task )
{
  if ( auto relation = cache_.get_relation( task ); relation.has_value() ) {
    VLOG( 1 ) << "Visiting relations" << endl;
    visit( relation.value(), [this]( Relation relation ) { this->storage().serialize( relation ); } );
  }
  return;
}
