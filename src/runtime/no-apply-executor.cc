#include "no-apply-executor.hh"
#include "applyer.hh"
#include "channel.hh"
#include "runner.hh"

using namespace std;

NoApplyExecutor::NoApplyExecutor( shared_ptr<Runner> runner, size_t threads, weak_ptr<IRuntime> parent )
  : Executor( 0, parent, make_shared<PointerRunner>() )
  , applyer_( make_shared<Applyer>( threads - 1, runner ) )
{
  thread_ = thread( [&]() { run(); } );
}

NoApplyExecutor::~NoApplyExecutor()
{
  apply_results_.close();
  thread_.join();
}

void NoApplyExecutor::run()
{
  try {
    Handle<Relation> todo;
    unique_ptr<ApplyResult> apply_result;

    while ( true ) {
      while ( !todo_.empty() ) {
        todo = todo_.front();
        todo_.pop();

        progress( todo );
      }

      apply_results_ >> apply_result;
      progress( std::move( apply_result ) );
      todo_.swap( blocked_todo_ );
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}

void NoApplyExecutor::progress( Handle<Relation> relation )
{
  auto result = evaluator_.relate( relation );
  if ( !result ) {
    blocked_todo_.push( relation );
  }
}

void NoApplyExecutor::progress( unique_ptr<ApplyResult> result )
{
  auto goal = Handle<Apply>( result->handle );
  storage_.create( goal, result->result );
  live_.erase( goal );

  for ( const auto& [h, d] : result->minrepo_blobs ) {
    if ( !storage_.contains( h ) ) {
      storage_.create( d, h );
    }
  }

  for ( const auto& [h, d] : result->minrepo_trees ) {
    if ( !storage_.contains( h ) ) {
      storage_.create( d, h );
    }
  }
}

std::optional<Handle<Object>> NoApplyExecutor::get_or_delegate( Handle<Relation> goal )
{
  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  }
  auto parent = parent_.lock();
  if ( parent ) {
    return parent->get( goal );
  } else {
    todo_.push( goal );
    return {};
  }
}

std::optional<Handle<Object>> NoApplyExecutor::get( Handle<Relation> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  }
  todo_.push( name );
  return {};
}

std::optional<Handle<Object>> NoApplyExecutor::apply( Handle<ObjectTree> combination )
{
  Handle<Apply> goal( combination );

  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  }

  if ( live_.contains( goal ) ) {
    return {};
  }

  // Load minrepo to memory
  load_minrepo( combination );
  auto [minrepo_blobs, minrepo_trees] = collect_minrepo( combination );

  auto result = runner_->apply( combination, std::move( minrepo_blobs ), std::move( minrepo_trees ) );

  if ( result.has_value() ) {
    storage_.create( goal, result.value() );
    live_.erase( goal );
  }

  return result;
}
