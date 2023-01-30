#include "worker.hh"
#include "runtimestorage.hh"
#include "wasm-rt-content.h"

Name RuntimeWorker::force_thunk( Name name )
{
  Name current_name = name;
  while ( true ) {
    Name new_name = reduce_thunk( current_name );
    switch ( new_name.get_content_type() ) {
      case ContentType::Blob:
      case ContentType::Tree:
        return force( new_name );

      default:
        current_name = new_name;
    }
  }
}

Name RuntimeWorker::force( Name name )
{
  if ( name.is_literal_blob() ) {
    return name;
  } else {
    switch ( name.get_content_type() ) {
      case ContentType::Blob:
        return name;

      case ContentType::Tree:
        return force_tree( name );

      case ContentType::Thunk:
        return force_thunk( name );

      default:
        throw std::runtime_error( "Invalid content type." );
    }
  }
}

Name RuntimeWorker::reduce_thunk( Name name )
{
  Name encode_name = runtimestorage_.get_thunk_encode_name( name );
  // Name canonical_name = local_to_storage( encode_name );
  // if ( memoization_cache.contains( canonical_name ) ) {
  //   return memoization_cache.at( canonical_name );
  // } else {
  Name result = evaluate_encode( encode_name );
  // memoization_cache.insert_or_assign( canonical_name, result );
  return result;
  //}
}

Name RuntimeWorker::force_tree( Name name ) // Accept Job
{
  // TODO need to be creating a new tree
  auto orig_tree = runtimestorage_.get_tree( name );

  std::atomic<size_t> pending_jobs = orig_tree.size();

  // Could just push all jobs all let it get taken care of by the pop
  for ( size_t i = 0; i < orig_tree.size(); ++i ) {
    auto entry = orig_tree[i];

    if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
      queue_job( std::move( Job( entry, &( orig_tree.mutable_data()[i] ), &pending_jobs ) ) );
    } else {
      pending_jobs--;
    }
  }

  while ( pending_jobs != 0 ) {
    Job job;
    bool work = dequeue_job( job );
    if ( work )
      compute_job( job );
  }

  return name;
}

Name RuntimeWorker::evaluate_encode( Name encode_name ) // Include Job
{
  force_tree( encode_name );
  Name function_name = runtimestorage_.get_tree( encode_name ).at( 1 );

  // TODO still need to create a new tree instead of updating in place

  if ( not function_name.is_blob() ) {
    throw std::runtime_error( "ENCODE functions not yet supported" );
  }

  Name canonical_name = runtimestorage_.local_to_storage( function_name );
  if ( not runtimestorage_.name_to_program_.contains( canonical_name ) ) {
    /* compile the Wasm to C and then to ELF */
    const auto [c_header, h_header, fixpoint_header]
      = wasmcompiler::wasm_to_c( runtimestorage_.get_blob( function_name ) );

    Program program = link_program( c_to_elf( c_header, h_header, fixpoint_header, wasm_rt_content ) );

    runtimestorage_.name_to_program_.insert_or_assign( canonical_name, std::move( program ) );
  }

  auto& program = runtimestorage_.name_to_program_.at( canonical_name );
  __m256i output = program.execute( encode_name );

  return output;
}

void RuntimeWorker::compute_job( Job& job )
{
  Name name = job.name;
  if ( !name.is_literal_blob() ) {
    switch ( name.get_content_type() ) {
      case ContentType::Blob:
        break;
      case ContentType::Tree:
        name = force_tree( name );
        break;

      case ContentType::Thunk:
        name = force_thunk( name );
        break;

      default:
        throw std::runtime_error( "Invalid content type." );
    }
  }

  *job.entry = name;
  ( *job.pending_jobs )--;
}

void RuntimeWorker::work()
{
  // Wait till all threads have been primed before computation can begin
  {
    std::unique_lock<std::mutex> conditional_lock( runtimestorage_.to_workers_mutex_ );
    runtimestorage_.to_workers_.wait( conditional_lock, [this] { return runtimestorage_.threads_active_; } );
  }

  while ( runtimestorage_.threads_active_ ) {
    Job job;
    bool work = dequeue_job( job );
    if ( work )
      compute_job( job );
  }
}

void RuntimeWorker::queue_job( Job job )
{
  // Add the job to our local queue and alert the runtime this job is READY
  // mgmt_.set_job_status( std::get<0>(job), JobManager::PENDING );
  jobs_.push( std::move( job ) );
}

bool RuntimeWorker::dequeue_job( Job& job )
{
  // Try to pop of the local queue, steal work if that fails, return false if no work can be found
  bool contains = jobs_.pop( job );
  if ( !contains ) {
    contains = runtimestorage_.steal_work( job, thread_id_ );
  }
  return contains;
}
