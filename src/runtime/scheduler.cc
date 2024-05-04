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
}

void LocalFirstScheduler::schedule( vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job )
{
  auto local = relater_.value().get().local_;
  if ( local->get_info()->parallelism > 0 ) {
    for ( auto leaf_job : leaf_jobs ) {
      leaf_job.visit<void>( overload {
        [&]( Handle<Literal> ) {},
        [&]( auto h ) { local->get( h ); },
      } );
    }
    return;
  }

  auto locked_remotes = relater_->get().remotes_.read();
  if ( locked_remotes->size() > 0 ) {
    for ( const auto& remote : locked_remotes.get() ) {
      auto locked_remote = remote.lock();
      if ( locked_remote ) {
        locked_remote->get( top_level_job );
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
                     // PassRunner::PassType::ChildBackProp,
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
