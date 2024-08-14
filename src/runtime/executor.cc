#include <glog/logging.h>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "executor.hh"
#include "fixpointapi.hh"
#include "overload.hh"
#include "resource_limits.hh"
#include "storage_exception.hh"

template<typename T>
using Result = Executor::Result<T>;

using namespace std;

Executor::Executor( Relater& parent, size_t threads, optional<shared_ptr<Runner>> runner )
  : parent_( parent )
  , runner_( runner.has_value() ? runner.value()
                                : make_shared<WasmRunner>( parent.labeled( "compile-elf" ),
                                                           parent.labeled( "compile-fixed-point" ) ) )
{
  for ( size_t i = 0; i < threads; i++ ) {
    threads_.emplace_back( [&]() {
      fixpoint::storage = &parent_.storage_;
      resource_limits::available_bytes = 0;
      run();
    } );
  }
}

Executor::~Executor()
{
  todo_.close();
  for ( size_t i = 0; i < threads_.size(); i++ ) {
    threads_[i].join();
  }
}

void Executor::run()
{
  static std::mutex error_mutex;
  Handle<Relation> next;
  try {
    while ( true ) {
      todo_ >> next;
      progress( next );
    }
  } catch ( StorageException& e ) {
    std::unique_lock lock( error_mutex );
    cerr << "--- STORAGE ERROR ---\n";
    cerr << "what: " << e.what() << endl;

    auto graph = parent_.graph_.write();
    cerr << "backtrace:\n";
    int i = 0;
    auto current = next;
    cerr << ++i << ". ";
    while ( true ) {
      current.visit<void>( [&]( auto x ) { cerr << x; } );
      cerr << endl;
      i++;
      absl::flat_hash_set<Handle<Relation>> unblocked;
      graph->finish( current, unblocked );
      if ( unblocked.empty() ) {
        break;
      }
      cerr << i << ". ";
      if ( unblocked.size() >= 2 ) {
        cerr << std::format( "[1/{}] ", unblocked.size() );
      }
      current = *unblocked.begin();
    }

    cerr << "---------------------\n";
    std::terminate();
  } catch ( ChannelClosed& ) {
    return;
  }
}

void Executor::progress( Handle<Relation> runnable )
{
  VLOG( 2 ) << "Progressing " << runnable;
  parent_.run( runnable );
}

Result<Object> Executor::apply( Handle<ObjectTree> combination )
{
  VLOG( 2 ) << "Apply " << combination;

  TreeData tree = parent_.storage_.get( combination );
  auto result = runner_->apply( combination, tree );

  return result;
}

std::optional<BlobData> Executor::get( Handle<Named> name )
{
  throw HandleNotFound( name );
};

std::optional<TreeData> Executor::get( Handle<AnyTree> name )
{
  throw HandleNotFound( handle::fix( name ) );
};

std::optional<Handle<Object>> Executor::get( Handle<Relation> name )
{
  if ( threads_.size() == 0 ) {
    throw HandleNotFound( name );
  }
  auto graph = parent_.graph_.write();
  if ( graph->start( name ) )
    todo_.push( name );
  return {};
}

std::optional<Handle<AnyTree>> Executor::get_handle( Handle<AnyTree> )
{
  return {};
}

std::optional<TreeData> Executor::get_shallow( Handle<AnyTree> )
{
  return {};
}

void Executor::put( Handle<Named> name, BlobData data )
{
  parent_.put( name, data );
}

void Executor::put( Handle<AnyTree> name, TreeData data )
{
  parent_.put( name, data );
}

void Executor::put_shallow( Handle<AnyTree> name, TreeData data )
{
  parent_.put_shallow( name, data );
}

void Executor::put( Handle<Relation> name, Handle<Object> data )
{
  parent_.put( name, data );
}

bool Executor::contains( Handle<Named> handle )
{
  return parent_.contains( handle );
}

bool Executor::contains( Handle<AnyTree> handle )
{
  return parent_.contains( handle );
}

bool Executor::contains_shallow( Handle<AnyTree> handle )
{
  return parent_.contains_shallow( handle );
}

bool Executor::contains( Handle<Relation> handle )
{
  return parent_.contains( handle );
}

Handle<Fix> Executor::labeled( const std::string_view label )
{
  return parent_.labeled( label );
};

bool Executor::contains( const std::string_view label )
{
  return parent_.contains( label );
}
