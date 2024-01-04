#include <string_view>
#include <utility>

#include "handle.hh"
#include "operation.hh"
#include "runtime.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "task.hh"

#include "wasm-rt-content.h"

#include <glog/logging.h>

using namespace std;

#ifndef COMPILE
#define COMPILE "COMPILE MUST BE DEFINED"
#endif

optional<Handle> RuntimeWorker::await( Task target, Task current )
{
  return graph_.start_after( target, current );
}

bool RuntimeWorker::await_tree( Task task )
{
  Tree tree = storage_.get_tree( task.handle() );
  return graph_.start_after( tree, task );
}

std::optional<Handle> RuntimeWorker::do_eval( Task task )
{
  Handle handle = task.handle();
  if ( handle.is_lazy() ) {
    return handle;
  }
  optional<Handle> result;

  switch ( handle.get_content_type() ) {
    case ContentType::Blob: {
      return handle;
    }

    case ContentType::Tree: {
      if ( !await_tree( task ) )
        return {};

      return await( Task::Fill( handle ), task );
    }

    case ContentType::Tag: {
      Handle tree = handle.as_tree();
      result = await( Task::Eval( tree ), task );
      if ( not result )
        return {};

      return result->as_tag();
    }

    case ContentType::Thunk: {
      Handle encode = handle.as_tree();

      result = await( Task::Eval( encode ), task );
      if ( not result )
        return {};

      result = await( Task::Apply( result.value() ), task );
      if ( not result )
        return {};

      if ( handle.is_shallow() )
        result = Handle::make_shallow( result.value() );

      return await( Task::Eval( result.value() ), task );
    }
  }

  throw std::runtime_error( "unhandled case in eval" );
}

Handle RuntimeWorker::do_apply( Task task )
{
  auto name = task.handle();
  CHECK( not name.is_thunk() );
  CHECK( name.is_strict() );
  CHECK( name.is_tree() );
  Handle function_tag = storage_.get_tree( name )[1];
  CHECK( not function_tag.is_blob() );

  while ( !storage_.get_tree( function_tag )[1].is_blob() ) {
    if ( not storage_.get_tree( function_tag )[1].is_tag() ) {
      throw std::runtime_error( "Procedure is not a tag." );
    }
    function_tag = storage_.get_tree( function_tag )[1];
  }

  auto tag = storage_.get_tree( function_tag );
  const static Handle COMPILE_ELF = storage_.get_ref( "compile-elf" ).value();
  if ( tag[0] != COMPILE_ELF ) {
    throw std::runtime_error( "Procedure is not generated by trusted compilation toolchain: generated by "
                              + storage_.get_display_name( tag[0] ) );
  }
  if ( not storage_.compare_handles( tag[2], Handle( "Runnable" ) ) ) {
    cerr << "Attempted to run non-Runnable procedure:" << endl;
    cerr << "- object: " << tag[1] << endl;
    cerr << "- author: " << tag[0] << endl;
    cerr << "- type: " << std::string_view( tag[2].literal_blob().data(), tag[2].get_length() ) << endl;
    Handle handle = tag[1];
    Blob data = storage_.get_blob( handle );
    bool is_printable
      = std::count_if( data.begin(), data.end(), []( unsigned char c ) { return std::isprint( c ); } );
    if ( is_printable == data.size() ) {
      cerr << "--- ERROR ---" << endl;
      cerr << string_view( data.data(), data.size() ) << endl;
      cerr << "-------------" << endl;
    } else {
      cerr << "Object is not printable." << endl;
    }
    throw std::runtime_error( "Procedure is not runnable" );
  }

  Handle function_name = tag[1];
  Handle canonical_name = storage_.canonicalize( function_name );
  const Program& program = storage_.link( function_name );

  runtime_.set_current_procedure( canonical_name );
  __m256i output = program.execute( name );

  return output;
}

Handle RuntimeWorker::do_fill( Handle name )
{
  switch ( name.get_content_type() ) {
    case ContentType::Tree: {
      auto orig_tree = storage_.get_tree( name );
      auto tree = OwnedMutTree::allocate( orig_tree.size() );
      VLOG( 3 ) << "filling " << tree.size() << " elements";
      for ( size_t i = 0; i < tree.size(); ++i ) {
        const auto entry = orig_tree[i];
        Handle evalled = graph_.get( Task::Eval( entry ) ).value();
        VLOG( 3 ) << "filling " << name << "[" << i << "] = " << evalled;
        tree[i] = evalled;
      }

      return storage_.add_tree( std::move( tree ) );
    }

    default:
      throw std::runtime_error( "Invalid content type for fill." );
  }
}

optional<Handle> RuntimeWorker::progress( Task task )
{
  switch ( task.operation() ) {
    case Operation::Apply:
      return do_apply( task );
    case Operation::Eval:
      return do_eval( task );
    case Operation::Fill:
      return do_fill( task.handle() );
  }
  throw std::runtime_error( "invalid operation for progress" );
}

void RuntimeWorker::work()
{
  current_thread_id_ = thread_id_;

  try {
    while ( true ) {
      auto task = runq_.pop_or_wait();
      VLOG( 1 ) << "Starting " << task;
      auto result = progress( task );
      if ( result ) {
        VLOG( 2 ) << "Got " << task << " = " << *result;
        graph_.finish( std::move( task ), *result );
      } else {
        VLOG( 2 ) << "Did not finish " << task;
      }
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}
