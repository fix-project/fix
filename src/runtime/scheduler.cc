#include "scheduler.hh"
#include "executor.hh"
#include "handle.hh"
#include "overload.hh"
#include "pass.hh"
#include "relater.hh"
#include "types.hh"
#include <memory>
#include <optional>
#include <stdexcept>

using namespace std;

LocalScheduler::Result<Fix> LocalScheduler::load( Handle<AnyDataType> handle )
{
  handle.visit<void>( overload { []( Handle<Literal> ) {},
                                 []( Handle<Relation> ) { throw std::runtime_error( "Invalid load()." ); },
                                 [&]( auto h ) { relater_->get().get( h ); } } );

  return handle::fix( handle );
}

SketchGraphScheduler::Result<Fix> SketchGraphScheduler::load( Handle<AnyDataType> handle )
{
  auto contained
    = handle.visit<bool>( overload { [&]( Handle<Literal> ) { return true; },
                                     [&]( auto h ) { return relater_->get().get_storage().contains( h ); } } );

  handle.visit<void>(
    overload { []( Handle<Literal> ) {},
               [&]( auto ) { sketch_graph_.add_dependency( current_schedule_step_.value(), handle ); } } );

  if ( contained ) {
    return handle::fix( handle );
  } else {
    works_.push_back( handle );
    return {};
  }
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

  if ( relater_->get().get_storage().contains( h ) ) {
    return h;
  } else {
    works_.push_back( d );
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
  auto apply = Handle<Relation>( Handle<Apply>( combination ) );
  if ( relater_->get().contains( apply ) ) {
    return relater_->get().get( apply ).value();
  }

  if ( !nested_ ) {
    return dynamic_pointer_cast<Executor>( relater_->get().get_local() )->apply( combination );
  } else {
    {
      auto graph = relater_->get().graph_.write();

      if ( !relater_->get().contains( apply ) ) {
        graph->add_dependency( current_schedule_step_.value(), apply );
      } else {
        return relater_->get().get( apply );
      }
    }

    relater_->get().get_local()->get( apply );

    return {};
  }
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::apply( Handle<ObjectTree> combination )
{
  auto apply = Handle<Relation>( Handle<Apply>( combination ) );
  // XXX
  if ( apply != current_schedule_step_.value() ) {
    sketch_graph_.add_dependency( current_schedule_step_.value(), apply );
  }

  if ( relater_->get().contains( apply ) ) {
    return relater_->get().get( apply ).value();
  }

  works_.push_back( Handle<Apply>( combination ) );
  return {};
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

  relater_->get().put( goal, *result );
  return result;
}

// XXX: all walking of the tree happens on the same thread
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
  current_schedule_step_ = goal;

  result = evaluator_.evalStrict( expression );

  current_schedule_step_ = prev_current;

  if ( result.has_value() ) {
    relater_->get().put( goal, result.value() );
  }

  if ( current_schedule_step_.has_value()
       && ( !result.has_value() || Handle<Object>( result.value() ) != expression ) ) {
    sketch_graph_.add_dependency( current_schedule_step_.value(), goal );
  }

  return result;
}

LocalScheduler::Result<Object> LocalScheduler::evalShallow( Handle<Object> expression )
{
  return evaluator_.evalShallow( expression );
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::evalShallow( Handle<Object> expression )
{
  return evaluator_.evalShallow( expression );
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
  for ( const auto& x : data->span() ) {
    auto obj = x.unwrap<Expression>().unwrap<Object>();
    auto result = evalStrict( obj );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && Handle<Object>( result.value() ) != obj ) {
      toreplace = true;
    }
  }
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

  for ( const auto& x : data->span() ) {
    auto exp = x.unwrap<Expression>();
    auto result = evaluator_.reduce( exp );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && x != Handle<Fix>( Handle<Expression>( result.value() ) ) ) {
      toreplace = true;
    }
  }

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

SketchGraphScheduler::Result<ValueTree> LocalScheduler::mapLift( Handle<ValueTree> tree )
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

  for ( const auto& x : data->span() ) {
    auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
    auto result = evaluator_.lift( val );
    if ( not result ) {
      ready = false;
    } else if ( !toreplace && result.value() != val ) {
      toreplace = true;
    }
  }

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
      [&]( Handle<Relation> r ) {
        r.visit<void>( overload {
          [&]( Handle<Eval> e ) { merge_all_sketch_graph( e, unblocked ); },
          []( Handle<Apply> ) {},
        } );
      },
      []( auto ) {},
    } );
  }
}

void SketchGraphScheduler::run_passes( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  VLOG( 1 ) << "HintScheduler input: " << top_level_job << endl;
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid run_passes() invocation." );
    return;
  }

  if ( relater_->get().remotes_.read()->size() == 0 ) {
    absl::flat_hash_set<Handle<Relation>> unblocked;
    merge_all_sketch_graph( top_level_job, unblocked );

    for ( auto leaf_job : leaf_jobs ) {
      leaf_job.visit<void>( overload {
        [&]( Handle<Literal> ) {},
        [&]( auto h ) { relater_->get().get_local()->get( h ); },
      } );
    }

    for ( auto job : unblocked ) {
      relater_->get().get_local()->get( job );
    }

    return;
  }

  PassRunner::run( relater_.value(), *this, top_level_job, passes_ );
}

void SketchGraphScheduler::relate( Handle<Relation> top_level_job )
{
  works_ = {};
  sketch_graph_ = {};

  evaluator_.relate( top_level_job );
}

LocalScheduler::Result<Object> LocalScheduler::schedule( Handle<Relation> top_level_job )
{
  nested_ = false;
  return evaluator_.relate( top_level_job );
}

SketchGraphScheduler::Result<Object> SketchGraphScheduler::schedule( Handle<Relation> top_level_job )
{
  works_ = {};
  sketch_graph_ = {};

  auto res = evaluator_.relate( top_level_job );

  if ( res.has_value() ) {
    return res;
  } else {
    run_passes( works_, top_level_job );
    return {};
  }
}
