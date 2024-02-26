#include <glog/logging.h>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "executor.hh"
#include "fixpointapi.hh"
#include "overload.hh"
#include "storage_exception.hh"

template<typename T>
using Result = Executor::Result<T>;

using namespace std;

Executor::Executor( Relater& parent, size_t threads, optional<shared_ptr<Runner>> runner )
  : parent_( parent )
  , runner_( runner.has_value() ? runner.value() : make_shared<WasmRunner>( parent.labeled( "compile-elf" ) ) )
{
  for ( size_t i = 0; i < threads; i++ ) {
    threads_.emplace_back( [&]() {
      current_ = {};
      fixpoint::storage = &parent_.storage_;
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
  try {
    Handle<Relation> next;
    while ( true ) {
      todo_ >> next;
      progress( next );
    }
  } catch ( StorageException& e ) {
    std::unique_lock lock( error_mutex );
    cerr << "--- STORAGE ERROR ---\n";
    cerr << "what: " << e.what() << endl;
    if ( current_ ) {
      auto graph = parent_.graph_.write();
      cerr << "backtrace:\n";
      int i = 0;
      auto current = current_.value();
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
    }
    cerr << "---------------------\n";
    std::terminate();
  } catch ( ChannelClosed& ) {
    return;
  }
}

void Executor::progress( Handle<Relation> relation )
{
  if ( parent_.contains( relation ) ) {
    return;
  }

  relation.visit<void>( overload { [&]( Handle<Apply> a ) { apply( a.unwrap<ObjectTree>() ); },
                                   [&]( Handle<Eval> e ) {
                                     handle::extract<Identification>( e.unwrap<Object>() )
                                       .transform( [&]( auto i ) {
                                         auto res = load( i.template unwrap<Value>() );
                                         put( e, res.value() );
                                         return e;
                                       } )
                                       .or_else( [&]() -> optional<Handle<Eval>> {
                                         parent_.get( relation );
                                         return e;
                                       } );
                                   } } );
}

Result<Value> Executor::load( Handle<Value> value )
{
  VLOG( 2 ) << "Loading " << value;
  return value.visit<Result<Value>>( overload {
    [&]( Handle<Blob> x ) -> Result<Blob> {
      if ( x.contains<Literal>() )
        return x;

      if ( parent_.storage_.contains( x.unwrap<Named>() ) )
        return x;

      return get( x.unwrap<Named>() ).transform( [&]( auto ) { return x; } );
    },
    [&]( Handle<ValueTree> x ) -> Result<Value> {
      if ( parent_.storage_.contains( x ) )
        return x;

      return get( x ).and_then( [&]( auto t ) -> Result<Value> {
        bool all_loaded = true;
        for ( const auto& handle : t->span() ) {
          all_loaded
            = all_loaded
              && handle::extract<Value>( handle ).and_then( [&]( auto h ) { return load( h ); } ).has_value();
        }
        if ( all_loaded ) {
          return x;
        } else {
          return {};
        }
      } );
    },
    [&]( auto x ) { return x; },
  } );
}

Result<Object> Executor::apply( Handle<ObjectTree> combination )
{
  VLOG( 2 ) << "Apply " << combination;
  Handle<Apply> goal( combination );

  TreeData tree = parent_.storage_.get( combination );
  auto result = runner_->apply( combination, tree );

  put( goal, result );
  return result;
}

std::optional<BlobData> Executor::get( Handle<Named> name )
{
  auto res = parent_.get( name );
  if ( res.has_value() )
    return res;

  throw HandleNotFound( name );
};

std::optional<TreeData> Executor::get( Handle<AnyTree> name )
{
  auto res = parent_.get( name );
  if ( res.has_value() )
    return res;

  throw HandleNotFound( handle::fix( name ) );
};

std::optional<Handle<Object>> Executor::get( Handle<Relation> name )
{
  if ( threads_.size() == 0 ) {
    throw HandleNotFound( name );
  }
  auto graph = parent_.graph_.write();
  if ( graph->start( name ) )
    todo_ << name;
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
