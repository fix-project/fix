#include "scheduler.hh"
#include "handle.hh"
#include "overload.hh"
#include "pass.hh"
#include "relater.hh"
#include "types.hh"
#include <optional>
#include <stdexcept>

using namespace std;

void Scheduler::merge_sketch_graph( Handle<Relation> job, absl::flat_hash_set<Handle<Relation>>& unblocked )
{
  if ( relater_.value().get().contains( job ) ) {
    return;
  }

  relater_.value().get().merge_sketch_graph( job, unblocked );

  if ( unblocked.contains( job ) ) {
    return;
  }

  for ( auto d : relater_.value().get().get_forward_dependencies( job ) ) {
    d.visit<void>( overload {
      [&]( Handle<Relation> r ) {
        r.visit<void>( overload {
          [&]( Handle<Eval> e ) { merge_sketch_graph( e, unblocked ); },
          []( Handle<Apply> ) {},
        } );
      },
      []( auto ) {},
    } );
  }
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

  if ( relater_->get().remotes_.read()->size() == 0 ) {
    absl::flat_hash_set<Handle<Relation>> unblocked;
    merge_sketch_graph( top_level_job, unblocked );

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

  PassRunner::run( relater_.value(),
                   top_level_job,
                   { PassRunner::PassType::MinAbsentMaxParallelism,
                     PassRunner::PassType::ChildBackProp,
                     PassRunner::PassType::InOutSource } );
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
