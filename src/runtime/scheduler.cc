#include "scheduler.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "storage_exception.hh"
#include <optional>

using namespace std;

void LocalFirstScheduler::schedule( SharedMutex<vector<weak_ptr<IRuntime>>>& remotes,
                                    shared_ptr<IRuntime> local,
                                    [[maybe_unused]] SharedMutex<DependencyGraph>& graph,
                                    vector<Handle<Relation>>& leaf_jobs,
                                    Handle<Relation> top_level_job,
                                    Relater& rt )
{
  if ( local->get_info()->parallelism > 0 ) {
    for ( const auto& leaf_job : leaf_jobs ) {
      local->get( leaf_job );
    }
    return;
  }

  auto locked_remotes = remotes.read();
  if ( locked_remotes->size() > 0 ) {
    for ( const auto& remote : locked_remotes.get() ) {
      auto locked_remote = remote.lock();
      if ( locked_remote ) {
        auto send = [&]( Handle<AnyDataType> h ) {
          h.visit<void>( overload { []( Handle<Literal> ) {},
                                    []( Handle<Relation> ) {},
                                    [&]( auto x ) { locked_remote->put( x, rt.get( x ).value() ); } } );
        };

        rt.visit( top_level_job.visit<Handle<Fix>>(
                    overload { []( Handle<Apply> a ) { return a.unwrap<ObjectTree>(); },
                               []( Handle<Eval> e ) {
                                 auto obj = e.unwrap<Object>();
                                 return handle::extract<Thunk>( obj )
                                   .transform( []( auto obj ) -> Handle<Fix> { return Handle<Strict>( obj ); } )
                                   .or_else( [&]() -> optional<Handle<Fix>> { return obj; } )
                                   .value();
                               } } ),
                  send );

        locked_remote->get( top_level_job );
        return;
      }
    }
  }

  throw HandleNotFound( top_level_job );
}
