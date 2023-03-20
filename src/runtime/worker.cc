#include <string_view>

#include "job.hh"
#include "name.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

void RuntimeWorker::queue_job( Job job )
{
  runtimestorage_.work_++;
  jobs_.push( std::move( job ) );
  runtimestorage_.work_.notify_one();
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
      Name new_name = runtimestorage_.add_tree( std::move( Tree( orig_tree.size() ) ) );
      Name nhash( new_name, false, { EVAL } );

      runtimestorage_.fix_cache_.insert_or_update( nhash, hash, orig_tree.size() );

      std::shared_ptr<std::atomic<int64_t>> pending = runtimestorage_.fix_cache_.get_pending( nhash );
      span_view<Name> tree = runtimestorage_.get_tree( new_name );

      for ( size_t i = 0; i < orig_tree.size(); ++i ) {
        auto entry = orig_tree[i];
        tree.mutable_data()[i] = entry;

        if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
          Name desired( entry, false, { EVAL } );
          Name operations( entry, true, { EVAL } );

          runtimestorage_.fix_cache_.insert_next( desired, new_name, 0 );

          queue_job( Job( entry, operations ) );
        } else {
          ( *( pending.get() ) )--;
        }
      }

      int64_t last_job = 0;
      if ( pending.get()->compare_exchange_strong( last_job, -1 ) ) {
        update_parent( new_name );
        progress( hash, new_name );
      }
      break;
    }

    case ContentType::Thunk: {
      Name desired( name, false, { FORCE, EVAL } );
      Name operations( name, true, { EVAL, FORCE } );

      runtimestorage_.fix_cache_.insert_or_update( desired, hash, 0 );

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

      runtimestorage_.fix_cache_.insert_or_update( desired, hash, 0 );

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
    throw std::runtime_error( "ENCODE functions not yet supported" );
  }

  Name canonical_name = runtimestorage_.local_to_storage( function_name );

  if ( not runtimestorage_.name_to_program_.contains( canonical_name ) ) {
    /* compile the Wasm to C and then to ELF */
    const auto [c_header, h_header, fixpoint_header]
      = wasmcompiler::wasm_to_c( runtimestorage_.get_blob( function_name ) );
    Program program = link_program( c_to_elf( c_header, h_header, fixpoint_header, wasm_rt_content ) );
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

void RuntimeWorker::child( Name hash )
{
  for ( int i = 1;; ++i ) {
    hash.set_index( i );
    if ( runtimestorage_.fix_cache_.contains( hash ) ) {
      Name tree_name = runtimestorage_.fix_cache_.get_name( hash );
      Name nhash( tree_name, false, { EVAL } );
      std::shared_ptr<std::atomic<int64_t>> pending = runtimestorage_.fix_cache_.get_pending( nhash );

      ( *pending.get() )--;

      int64_t last_job = 0;
      if ( pending.get()->compare_exchange_strong( last_job, -1 ) ) {
        update_parent( tree_name );
        // this can be a seperate job if needed
        progress( runtimestorage_.fix_cache_.get_name( nhash ), tree_name );
      }
    } else {
      break;
    }
  }
}

void RuntimeWorker::progress( Name hash, Name name )
{
  uint32_t current = hash.peek_operation();

  if ( current != NONE ) {
    hash.pop_operation();
    Name nhash( name, false, { current } );
    runtimestorage_.fix_cache_.insert_or_update( nhash, hash, 1 );

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
      default:
        throw std::runtime_error( "Invalid job type." );
    }
  } else {
    if ( runtimestorage_.fix_cache_.contains( hash ) ) {
      Name requestor = runtimestorage_.fix_cache_.get_name( hash );
      runtimestorage_.fix_cache_.insert_or_update( hash, name, 0 );

      if ( requestor != hash ) {
        progress( requestor, name );
      } else {
        child( requestor );
      }
    }
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
