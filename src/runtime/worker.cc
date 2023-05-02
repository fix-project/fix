#include <string_view>
#include <utility>

#include "base64.hh"
#include "job.hh"
#include "name.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

void RuntimeWorker::queue_job( Job job )
{
  runtimestorage_.work_++;
  jobs_.push( std::move( job ) );
  runtimestorage_.work_.notify_all();
}

bool RuntimeWorker::dequeue_job( Job& job )
{
  // Try to pop off the local queue, steal work if that fails, return false if no work can be found
  bool work = jobs_.pop( job );

  if ( !work )
    work = runtimestorage_.steal_work( job, thread_id_ );

  if ( work )
    runtimestorage_.work_--;

  return work;
}

void RuntimeWorker::eval( Name hash, Name name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      progress( hash, name );
      break;
    }

    case ContentType::Tree: {
      auto orig_tree = runtimestorage_.get_tree( name );
      Name fill_operation( name, true, { FILL } );
      Name fill_desired( name, false, { FILL } );

      runtimestorage_.fix_cache_.pending_start( fill_operation, name, orig_tree.size() );

      std::shared_ptr<std::atomic<int64_t>> pending = runtimestorage_.fix_cache_.get_pending( fill_operation );
      bool to_fill = false;

      for ( size_t i = 0; i < orig_tree.size(); ++i ) {
        auto entry = orig_tree[i];

        if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
          to_fill = true;

          Name desired( entry, false, { EVAL } );
          Name operations( entry, true, { EVAL } );

          if ( runtimestorage_.fix_cache_.start_after( desired, fill_operation ) ) {
            queue_job( Job( entry, operations ) );
          } else {
            ( *pending )--;
          }
        } else {
          ( *pending )--;
        }
      }

      if ( !to_fill ) {
        progress( hash, name );
        return;
      }

      auto value_if_completed = runtimestorage_.fix_cache_.continue_after( fill_desired, hash );
      if ( value_if_completed.has_value() ) {
        progress( hash, value_if_completed.value() );
      } else {
        if ( pending->load() == 0 ) {
          progress( fill_operation, name );
        }
      }
      break;
    }

    case ContentType::Thunk: {
      Name desired( name, false, { FORCE, EVAL } );
      Name operations( name, true, { EVAL, FORCE } );

      runtimestorage_.fix_cache_.continue_after( desired, hash );

      progress( operations, name );

      // return eval(force(name))
      break;
    }

    default: {
      throw std::runtime_error( "Invalid content type." );
    }
  }
}

void RuntimeWorker::force( Name hash, Name name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Thunk: {
      Name encode_name = Name::get_encode_name( name );

      Name desired( encode_name, false, { EVAL, APPLY, FORCE } );
      Name operations( encode_name, true, { FORCE, APPLY, EVAL } );

      runtimestorage_.fix_cache_.continue_after( desired, hash );

      progress( operations, encode_name );

      // return force(apply(eval(encode_name)))
      break;
    }

    case ContentType::Blob:
    case ContentType::Tree: {
      progress( hash, name );
      break;
    }
    default: {
      throw std::runtime_error( "Invalid content type." );
    }
  }
}

void RuntimeWorker::apply( Name hash, Name name )
{
  Name function_name = runtimestorage_.get_tree( name ).at( 1 );
  if ( not function_name.is_blob() ) {
    if ( function_name.is_tree() ) {
      function_name = runtimestorage_.get_tree( function_name ).at( 0 );
      if ( not function_name.is_blob() ) {
        throw std::runtime_error( "Multilevel ENCODE functions not yet supported" );
      }
    } else {
      throw std::runtime_error( "Unevaled strict Thunk in apply(ENCODE). " );
    }
  }

  Name canonical_name = runtimestorage_.local_to_storage( function_name );
  if ( not runtimestorage_.name_to_program_.contains( canonical_name ) ) {
    /* Link program */
    Program program = link_program( runtimestorage_.get_blob( function_name ) );
    runtimestorage_.name_to_program_.put( canonical_name, std::move( program ) );
  }

  auto& program = runtimestorage_.name_to_program_.getMutable( canonical_name );
  __m256i output = program.execute( name );
  // Compute next operation
  progress( hash, output );
}

void RuntimeWorker::update_parent( Name name )
{
  span_view<Name> tree = runtimestorage_.get_tree( name );

  for ( size_t i = 0; i < tree.size(); ++i ) {
    auto entry = tree[i];
    if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
      Name desired( entry, false, { EVAL } );
      tree.mutable_data()[i] = runtimestorage_.fix_cache_.get_name( desired );
    }
  }
}

void RuntimeWorker::fill( Name hash, Name name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Tree: {
      auto orig_tree = runtimestorage_.get_tree( name );
      Name new_name = runtimestorage_.add_tree( Tree( orig_tree.size() ) );
      span_view<Name> tree = runtimestorage_.get_tree( new_name );

      for ( size_t i = 0; i < tree.size(); ++i ) {
        auto entry = orig_tree[i];
        tree.mutable_data()[i] = entry;

        if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
          Name desired( entry, false, { EVAL } );
          tree.mutable_data()[i] = runtimestorage_.fix_cache_.get_name( desired );
        }
      }

      progress( hash, new_name );
      return;
    }

    default:
      throw std::runtime_error( "Invalid job type." );
  }
}

void RuntimeWorker::launch_jobs( std::queue<std::pair<Name, Name>> ready_jobs )
{
  while ( !ready_jobs.empty() ) {
    queue_job( Job( ready_jobs.front().first, ready_jobs.front().second ) );
    ready_jobs.pop();
  }
}

void RuntimeWorker::progress( Name hash, Name name )
{
  uint32_t current = hash.peek_operation();

  if ( current != NONE ) {
    hash.pop_operation();
    Name nhash( name, false, { current } );
    int status = runtimestorage_.fix_cache_.try_run( nhash, hash );
    if ( status == 1 ) {
      switch ( current ) {
        case APPLY:
          apply( nhash, name );
          break;
        case EVAL:
          eval( nhash, name );
          break;
        case FORCE:
          force( nhash, name );
          break;
        case FILL:
          fill( nhash, name );
          break;
        default:
          throw std::runtime_error( "Invalid job type." );
      }
    } else if ( status == -1 ) {
      progress( hash, runtimestorage_.fix_cache_.get_name( nhash ) );
    }
  } else {
    launch_jobs( runtimestorage_.fix_cache_.complete( hash, name ) );
  }
}

void RuntimeWorker::work()
{
  // Wait till all threads have been primed before computation can begin
  runtimestorage_.threads_active_.wait( false );

  Job job;
  while ( runtimestorage_.threads_active_ ) {
    if ( dequeue_job( job ) ) {
      progress( job.hash, job.name );
    } else {
      runtimestorage_.work_.wait( 0 );
    }
  }
}
