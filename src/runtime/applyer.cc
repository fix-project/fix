#include "applyer.hh"

using namespace std;

void Applyer::run()
{
  try {
    ApplyEnv next;
    while ( true ) {
      todo_ >> next;
      progress( std::move( next ) );
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}

void Applyer::progress( ApplyEnv&& apply_todo )
{
  auto res = runner_->apply(
    apply_todo.handle, std::move( apply_todo.minrepo_blobs ), std::move( apply_todo.minrepo_trees ) );

  if ( res.has_value() ) {
    ApplyResult result { .handle = apply_todo.handle,
                         .result = res.value(),
                         .minrepo_blobs = std::move( curr_minrepo_blobs ),
                         .minrepo_trees = std::move( curr_minrepo_trees ) };
    result_.move_push( std::move( result ) );
  }
}

Applyer::Applyer( size_t threads, shared_ptr<Runner> runner )
  : runner_( runner )
{
  for ( size_t i = 0; i < threads; i++ ) {
    threads_.emplace_back( [&]() { run(); } );
  }
}

std::optional<Handle<Object>> Applyer::apply( Handle<ObjectTree> handle,
                                              BlobMap&& minrepo_blobs,
                                              TreeMap&& minrepo_trees )
{
  ApplyEnv env {
    .handle = handle, .minrepo_blobs = std::move( minrepo_blobs ), .minrepo_trees = std::move( minrepo_trees ) };
  todo_.move_push( std::move( env ) );
  return {};
}
