#include "scheduler.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "overload.hh"
#include "pass.hh"
#include "storage_exception.hh"
#include "types.hh"
#include <limits>
#include <optional>
#include <stdexcept>

using namespace std;

namespace scheduler {
bool is_local( shared_ptr<IRuntime> rt )
{
  return rt->get_info()->link_speed == numeric_limits<double>::max();
}

void get( shared_ptr<IRuntime> worker, Handle<AnyDataType> job, Relater& rt )
{
  if ( is_local( worker ) ) {
    job.visit<void>( overload {
      [&]( Handle<Literal> ) {},
      [&]( auto h ) { worker->get( h ); },
    } );
  } else {
    // Send visit( job ) before sending the job itself
    auto send = [&]( Handle<AnyDataType> h ) {
      h.visit<void>( overload { []( Handle<Literal> ) {},
                                []( Handle<Relation> ) {},
                                [&]( auto x ) { worker->put( x, rt.get( x ).value() ); } } );
    };

    job.visit<void>( overload {
      [&]( Handle<Relation> r ) {
        rt.visit( r.visit<Handle<Fix>>( overload { []( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
                                                   []( Handle<Eval> e ) {
                                                     auto obj = e.unwrap<Object>();
                                                     return handle::extract<Thunk>( obj )
                                                       .transform( []( auto obj ) -> Handle<Fix> {
                                                         return Handle<Strict>( obj );
                                                       } )
                                                       .or_else( [&]() -> optional<Handle<Fix>> { return obj; } )
                                                       .value();
                                                   } } ),
                  send );
      },
      []( auto ) {} } );

    job.visit<void>( overload { []( Handle<Literal> ) { throw runtime_error( "Unreachable" ); },
                                [&]( auto h ) { worker->get( h ); } } );
  }
}
}

void LocalFirstScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  auto local = relater_.value().get().local_;
  if ( local->get_info()->parallelism > 0 ) {
    for ( const auto& leaf_job : leaf_jobs ) {
      scheduler::get( local, leaf_job, relater_->get() );
    }
    return;
  }

  auto locked_remotes = relater_->get().remotes_.read();
  if ( locked_remotes->size() > 0 ) {
    for ( const auto& remote : locked_remotes.get() ) {
      auto locked_remote = remote.lock();
      if ( locked_remote ) {
        scheduler::get( locked_remote, top_level_job, relater_->get() );
        return;
      }
    }
  }

  throw HandleNotFound( top_level_job );
}

void OnePassScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  VLOG( 1 ) << "OnePassScheduler input: " << top_level_job << endl;
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid schedule() invocation." );
    return;
  }

  PassRunner::run( relater_.value(), top_level_job, { PassRunner::PassType::MinAbsentMaxParallelism } );
}

void HintScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  VLOG( 1 ) << "HintScheduler input: " << top_level_job << endl;
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid schedule() invocation." );
    return;
  }

  PassRunner::run( relater_.value(),
                   top_level_job,
                   { PassRunner::PassType::MinAbsentMaxParallelism,
                     PassRunner::PassType::ChildBackProp,
                     PassRunner::PassType::OutSource } );
}

void RandomScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  // If all dependencies are resolved, the job should have been completed
  if ( leaf_jobs.empty() ) {
    throw runtime_error( "Invalid schedule() invocation." );
    return;
  }

  PassRunner::run( relater_.value(), top_level_job, { PassRunner::PassType::Random } );
}
