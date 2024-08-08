#include "scheduler.hh"
#include "executor.hh"
#include "handle.hh"
#include "overload.hh"
#include "pass.hh"
#include "relater.hh"
#include "storage_exception.hh"
#include "types.hh"
#include "wasm-rt.h"
#include <memory>
#include <optional>
#include <stdexcept>

using namespace std;

LocalScheduler::Result<Blob> LocalScheduler::load( Handle<Blob> handle )
{
  handle.visit<void>( overload {
    []( Handle<Literal> ) {},
    [&]( Handle<Named> n ) { relater_->get().get( n ); },
  } );

  return handle;
}

LocalScheduler::Result<AnyTree> LocalScheduler::load( Handle<AnyTree> handle )
{
  relater_->get().get( handle );
  auto res = relater_->get().get_handle( handle ).value();

  return res;
}

LocalScheduler::Result<AnyTree> LocalScheduler::loadShallow( Handle<AnyTree> handle )
{
  relater_->get().get_shallow( handle );
  auto res = relater_->get().get_handle( handle ).value();

  return res;
}

SketchGraphScheduler::Result<Blob> SketchGraphScheduler::load( Handle<Blob> handle )
{
  return handle.visit<Result<Blob>>( overload {
    []( Handle<Literal> l ) { return l; },
    [&]( Handle<Named> n ) -> Result<Blob> {
      sketch_graph_.add_dependency( current_schedule_step_.value(), n );
      if ( relater_->get().contains( n ) ) {
        relater_->get().get( n );
        return n;
      } else {
        return {};
      }
    },
  } );
}

SketchGraphScheduler::Result<AnyTree> SketchGraphScheduler::load( Handle<AnyTree> handle )
{
  handle = relater_->get().get_handle( handle ).value();

  return handle.visit<Result<AnyTree>>( [&]( auto h ) -> Result<AnyTree> {
    sketch_graph_.add_dependency( current_schedule_step_.value(), h );

    if ( relater_->get().contains( h ) ) {
      relater_->get().get( h );
      return h;
    } else {
      return {};
    }
  } );
}

LocalScheduler::Result<AnyTree> LocalScheduler::load( Handle<AnyTreeRef> handle )
{
  auto h = relater_->get().unref( handle );
  relater_->get().get( h );
  return h;
}

SketchGraphScheduler::Result<AnyTree> SketchGraphScheduler::load( Handle<AnyTreeRef> handle )
{
  auto h = relater_->get().unref( handle );

  auto d = h.visit<Handle<AnyDataType>>( []( auto h ) { return h; } );
  sketch_graph_.add_dependency( current_schedule_step_.value(), d );

  if ( relater_->get().contains( h ) ) {
    relater_->get().get( h );
    return h;
  } else {
    return {};
  }
}

Handle<AnyTreeRef> LocalScheduler::ref( Handle<AnyTree> tree )
{
  return relater_->get().ref( tree );
}

Handle<AnyTreeRef> SketchGraphScheduler::ref( Handle<AnyTree> tree )
{
  return relater_->get().ref( tree );
}

LocalScheduler::Result<Object> LocalScheduler::apply( Handle<ObjectTree> combination )
{
  Handle<Relation> apply
    = Handle<Step>( Handle<Thunk>( Handle<Application>( Handle<ExpressionTree>( combination ) ) ) );

  if ( relater_->get().contains( apply ) ) {
    return relater_->get().get( apply ).value();
  }

  if ( !nested_ ) {
    return dynamic_pointer_cast<Executor>( relater_->get().get_local() )->apply( combination );
  } else {
    return {};
  }
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::apply( Handle<ObjectTree> combination )
{
  Handle<Relation> apply
    = Handle<Step>( Handle<Thunk>( Handle<Application>( Handle<ExpressionTree>( combination ) ) ) );

  if ( relater_->get().contains( apply ) ) {
    return relater_->get().get( apply ).value();
  }

  if ( !nested_ ) {
    return dynamic_pointer_cast<Executor>( relater_->get().get_local() )->apply( combination );
  }

  return {};
}

LocalScheduler::Result<Object> LocalScheduler::select( Handle<ObjectTree> combination )
{
  Handle<Relation> select = Handle<Step>( Handle<Thunk>( Handle<Selection>( combination ) ) );

  if ( relater_->get().contains( select ) ) {
    return relater_->get().get( select ).value();
  }

  auto tree = relater_->get().get( combination ).value();

  auto h = tree->at( 0 ).unwrap<Expression>().unwrap<Object>();
  auto size = handle::size( h );
  auto i = size_t(
    tree->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );

  if ( i >= size ) {
    throw std::runtime_error( "Select OOB" );
  }

  // h is one of BlobRef, ValueTreeRef or ObjectTreeRef
  auto result = h.visit<Result<Object>>( overload {
    [&]( Handle<ObjectTreeRef> x ) {
      return loadShallow( relater_->get().unref( x ) )
        .and_then( [&]( Handle<AnyTree> x ) { return relater_->get().get_shallow( x ); } )
        .transform( [&]( TreeData d ) { return d->span()[i].unwrap<Expression>().unwrap<Object>(); } );
    },
    [&]( Handle<Value> v ) {
      return v.visit<Result<Object>>( overload {
        [&]( Handle<Literal> l ) { return Handle<Literal>( l.view().substr( i, 1 ) ); },
        [&]( Handle<BlobRef> x ) {
          return load( x.unwrap<Blob>() )
            .and_then( [&]( Handle<Blob> x ) { return relater_->get().get( x.unwrap<Named>() ); } )
            .transform( [&]( BlobData b ) { return Handle<Literal>( b->span()[i] ); } );
        },
        [&]( Handle<ValueTreeRef> x ) {
          return loadShallow( relater_->get().unref( x ) )
            .and_then( [&]( Handle<AnyTree> x ) { return relater_->get().get_shallow( x ); } )
            .transform( [&]( TreeData d ) { return d->span()[i].unwrap<Expression>().unwrap<Object>(); } );
        },
        []( auto ) -> Result<Object> { throw std::runtime_error( "Invalid select" ); },
      } );
    },
    []( auto ) -> Result<Object> { throw std::runtime_error( "Invalid select" ); },
  } );

  return result;
}

LocalScheduler::Result<Value> LocalScheduler::evalStrict( Handle<Object> expression )
{
  Handle<Eval> goal( expression );
  if ( relater_->get().contains( goal ) ) {
    return relater_->get().get( goal )->unwrap<Value>();
  }

  auto prev_current = current_schedule_step_;
  auto prev_nested = nested_;

  current_schedule_step_ = goal;
  nested_ = prev_current.has_value();

  auto result = evaluator_.evalStrict( expression );

  current_schedule_step_ = prev_current;
  nested_ = prev_nested;

  if ( !result ) {
    if ( current_schedule_step_.has_value() ) {
      auto graph = relater_->get().graph_.write();

      if ( !relater_->get().contains( goal ) ) {
        graph->add_dependency( current_schedule_step_.value(), goal );
      } else {
        return relater_->get().get( goal )->unwrap<Value>();
      }
    }
    return {};
  }

  return result;
}

SketchGraphScheduler::Result<Value> SketchGraphScheduler::evalStrict( Handle<Object> expression )
{
  Handle<Eval> goal( expression );
  optional<Handle<Value>> result;

  if ( relater_->get().contains( goal ) ) {
    result = relater_->get().get( goal )->unwrap<Value>();
  }

  if ( result.has_value() ) {
    if ( current_schedule_step_.has_value() && Handle<Object>( result.value() ) != expression ) {
      sketch_graph_.add_dependency( current_schedule_step_.value(), goal );
    }
    return result.value();
  }

  auto prev_current = current_schedule_step_;
  auto prev_nested = nested_;

  current_schedule_step_ = goal;
  nested_ = prev_current.has_value();

  result = evaluator_.evalStrict( expression );

  current_schedule_step_ = prev_current;
  nested_ = prev_nested;

  if ( result.has_value() ) {
    relater_->get().put( goal, result.value() );
  }

  if ( current_schedule_step_.has_value()
       && ( !result.has_value() || Handle<Object>( result.value() ) != expression ) ) {
    sketch_graph_.add_dependency( current_schedule_step_.value(), goal );
  }

  return result;
}

LocalScheduler::Result<Object> LocalScheduler::force( Handle<Thunk> thunk )
{
  Handle<Step> goal( thunk );
  if ( relater_->get().contains( goal ) ) {
    return relater_->get().get( goal );
  }

  auto prev_current = current_schedule_step_;

  current_schedule_step_ = goal;

  auto result = evaluator_.force( thunk );

  current_schedule_step_ = prev_current;

  if ( !result ) {
    if ( current_schedule_step_.has_value() ) {
      auto graph = relater_->get().graph_.write();

      if ( !relater_->get().contains( goal ) ) {
        graph->add_dependency( current_schedule_step_.value(), goal );
      } else {
        return relater_->get().get( goal );
      }
    }

    if ( relater_->get().graph_.read()->get_forward_dependencies( goal ).empty() ) {
      relater_->get().get_local()->get( goal );
    }

    return {};
  }

  return result;
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::force( Handle<Thunk> thunk )
{
  Handle<Step> goal( thunk );

  if ( current_schedule_step_.has_value() ) {
    sketch_graph_.add_dependency( current_schedule_step_.value(), goal );
  }

  if ( relater_->get().contains( goal ) ) {
    return relater_->get().get( goal );
  }

  auto prev_current = current_schedule_step_;
  current_schedule_step_ = goal;

  auto result = evaluator_.force( thunk );

  current_schedule_step_ = prev_current;

  return result;
}

LocalScheduler::Result<ValueTree> LocalScheduler::mapEval( Handle<ObjectTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto obj = x.unwrap<Expression>().unwrap<Object>();
    auto result = evalStrict( obj );

    if ( not result ) {
      ready = false;
    } else if ( !toreplace && Handle<Object>( result.value() ) != obj ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto values = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto x = data->at( i );
      auto obj = x.unwrap<Expression>().unwrap<Object>();
      values[i] = evalStrict( obj ).value();
    }

    if ( tree.is_tag() ) {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( values ) ) )
        .unwrap<ValueTree>()
        .tag();
    } else {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( values ) ) )
        .unwrap<ValueTree>();
    }
  } else {
    return Handle<ValueTree>( tree.content, tree.size(), tree.is_tag() );
  }
}

SketchGraphScheduler::Result<ValueTree> SketchGraphScheduler::mapEval( Handle<ObjectTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto obj = x.unwrap<Expression>().unwrap<Object>();
    auto result = evalStrict( obj );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && Handle<Object>( result.value() ) != obj ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto values = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto x = data->at( i );
      auto obj = x.unwrap<Expression>().unwrap<Object>();
      values[i] = evalStrict( obj ).value();
    }

    if ( tree.is_tag() ) {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( values ) ) )
        .unwrap<ValueTree>()
        .tag();
    } else {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( values ) ) )
        .unwrap<ValueTree>();
    }
  } else {
    return Handle<ValueTree>( tree.content, tree.size(), tree.is_tag() );
  }
}

LocalScheduler::Result<ObjectTree> LocalScheduler::mapReduce( Handle<ExpressionTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto exp = x.unwrap<Expression>();
    auto result = evaluator_.reduce( exp );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && x != Handle<Fix>( Handle<Expression>( result.value() ) ) ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto objs = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto exp = data->at( i ).unwrap<Expression>();
      objs[i] = evaluator_.reduce( exp ).value();
    }

    if ( tree.is_tag() ) {
      return handle::tree_unwrap<ObjectTree>(
               relater_->get().get_storage().create( std::make_shared<OwnedTree>( std::move( objs ) ) ) )
        .tag();
    } else {
      return handle::tree_unwrap<ObjectTree>(
        relater_->get().get_storage().create( std::make_shared<OwnedTree>( std::move( objs ) ) ) );
    }
  } else {
    return Handle<ObjectTree>( tree.content, tree.size(), tree.is_tag() );
  }
}

SketchGraphScheduler::Result<ObjectTree> SketchGraphScheduler::mapReduce( Handle<ExpressionTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto exp = x.unwrap<Expression>();
    auto result = evaluator_.reduce( exp );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && x != Handle<Fix>( Handle<Expression>( result.value() ) ) ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto objs = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto exp = data->at( i ).unwrap<Expression>();
      objs[i] = evaluator_.reduce( exp ).value();
    }

    if ( tree.is_tag() ) {
      return handle::tree_unwrap<ObjectTree>(
               relater_->get().get_storage().create( std::make_shared<OwnedTree>( std::move( objs ) ) ) )
        .tag();
    } else {
      return handle::tree_unwrap<ObjectTree>(
        relater_->get().get_storage().create( std::make_shared<OwnedTree>( std::move( objs ) ) ) );
    }
  } else {
    return Handle<ObjectTree>( tree.content, tree.size(), tree.is_tag() );
  }
}

LocalScheduler::Result<ValueTree> LocalScheduler::mapLift( Handle<ValueTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
    auto result = evaluator_.lift( val );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && result.value() != val ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto vals = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto x = data->at( i );
      auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
      vals[i] = evaluator_.lift( val ).value();
    }

    if ( tree.is_tag() ) {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .unwrap<ValueTree>()
        .tag();
    } else {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .unwrap<ValueTree>();
    }
  } else {
    return tree;
  }
}

SketchGraphScheduler::Result<ValueTree> SketchGraphScheduler::mapLift( Handle<ValueTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
    auto result = evaluator_.lift( val );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && result.value() != val ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto vals = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto x = data->at( i );
      auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
      vals[i] = evaluator_.lift( val ).value();
    }

    if ( tree.is_tag() ) {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .unwrap<ValueTree>()
        .tag();
    } else {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .unwrap<ValueTree>();
    }
  } else {
    return tree;
  }
}

LocalScheduler::Result<ObjectTree> LocalScheduler::mapEvalShallow( Handle<ObjectTree> tree )
{
  bool ready = true;
  bool toreplace = false;
  auto data = relater_->get().get_shallow( tree ).value();

  auto prev_nested = nested_;
  nested_ = true;

  for ( const auto& x : data->span() ) {
    auto val = x.unwrap<Expression>().unwrap<Object>();
    auto result = evaluator_.evalShallow( val );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && result.value() != val ) {
      toreplace = true;
    }
  }

  nested_ = prev_nested;

  if ( not ready ) {
    return {};
  }

  if ( toreplace ) {
    auto vals = OwnedMutTree::allocate( data->size() );
    for ( size_t i = 0; i < data->size(); i++ ) {
      auto x = data->at( i );
      auto val = x.unwrap<Expression>().unwrap<Object>();
      vals[i] = evaluator_.evalShallow( val ).value();
    }

    if ( tree.is_tag() ) {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .visit<Handle<ObjectTree>>( overload {
          []( Handle<ValueTree> t ) { return t.into<ObjectTree>(); },
          []( Handle<ObjectTree> t ) { return t; },
          []( Handle<ExpressionTree> ) -> Handle<ObjectTree> { throw std::runtime_error( "Unreachable" ); } } )
        .tag();
    } else {
      return relater_->get()
        .get_storage()
        .create( std::make_shared<OwnedTree>( std::move( vals ) ) )
        .visit<Handle<ObjectTree>>( overload {
          []( Handle<ValueTree> t ) { return t.into<ObjectTree>(); },
          []( Handle<ObjectTree> t ) { return t; },
          []( Handle<ExpressionTree> ) -> Handle<ObjectTree> { throw std::runtime_error( "Unreachable" ); } } );
    }
  } else {
    return tree;
  }
}

void SketchGraphScheduler::merge_sketch_graph( Handle<Relation> r,
                                               absl::flat_hash_set<Handle<Relation>>& unblocked )
{
  auto graph = relater_->get().graph_.write();

  for ( auto d : sketch_graph_.get_forward_dependencies( r ) ) {
    auto contained
      = d.visit<bool>( overload { []( Handle<Literal> ) { return true; },
                                  [&]( auto h ) { return relater_->get().get_storage().contains( h ); } } );

    if ( contained ) {
      sketch_graph_.finish( d, unblocked );
    } else {
      graph->add_dependency( r, d );
    }
  }
}

void SketchGraphScheduler::merge_all_sketch_graph( Handle<Relation> job,
                                                   absl::flat_hash_set<Handle<Relation>>& unblocked )
{
  if ( relater_.value().get().contains( job ) ) {
    return;
  }

  merge_sketch_graph( job, unblocked );

  if ( unblocked.contains( job ) ) {
    return;
  }

  for ( auto d : relater_.value().get().get_forward_dependencies( job ) ) {
    d.visit<void>( overload {
      [&]( Handle<Relation> r ) { merge_all_sketch_graph( r, unblocked ); },
      []( auto ) {},
    } );
  }
}

optional<Handle<Object>> SketchGraphScheduler::run_passes( Handle<Relation> top_level_job )
{
  VLOG( 1 ) << "HintScheduler input: " << top_level_job << endl;

  if ( relater_->get().remotes_.read()->size() == 0 ) {
    absl::flat_hash_set<Handle<Relation>> unblocked;
    merge_all_sketch_graph( top_level_job, unblocked );

    if ( unblocked.size() == 1 ) {
      auto r = *unblocked.begin();
      auto thunk = r.visit<optional<Handle<Thunk>>>( overload { [&]( Handle<Eval> ) -> optional<Handle<Thunk>> {
                                                                 relater_->get().get_local()->get( r );
                                                                 return {};
                                                               },
                                                                [&]( Handle<Step> s ) -> optional<Handle<Thunk>> {
                                                                  if ( top_level_job == r ) {
                                                                    return s.unwrap<Thunk>();
                                                                  } else {
                                                                    relater_->get().get_local()->get( r );
                                                                    return {};
                                                                  }
                                                                } } );

      if ( thunk.has_value() ) {
        nested_ = false;
        go_for_it_ = true;

        return evaluator_.force( thunk.value() );
      }
    } else {
      for ( auto job : unblocked ) {
        relater_->get().get_local()->get( job );
      }
    }

    return {};
  }

  auto thunk = PassRunner::run( relater_.value(), *this, top_level_job, passes_ );

  if ( thunk.has_value() ) {
    nested_ = false;
    go_for_it_ = true;

    return evaluator_.force( thunk.value() );
  }

  return {};
}

void SketchGraphScheduler::relate( Handle<Relation> top_level_job )
{
  sketch_graph_ = {};

  evaluator_.relate( top_level_job );
}

LocalScheduler::Result<Object> LocalScheduler::schedule( Handle<Relation> top_level_job )
{
  nested_ = false;
  auto result = evaluator_.relate( top_level_job );

  if ( result.has_value() ) {
    relater_->get().put( top_level_job, *result );
  }

  return result;
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::schedule( Handle<Relation> top_level_job )
{
  nested_ = false;
  go_for_it_ = false;

  sketch_graph_ = {};

  auto res = evaluator_.relate( top_level_job );

  if ( res.has_value() ) {
    relater_->get().put( top_level_job, *res );
    return res;
  } else {
    auto res = run_passes( top_level_job );
    if ( res.has_value() ) {
      relater_->get().put( top_level_job, *res );
    }

    return {};
  }
}
