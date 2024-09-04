#include "runtimes.hh"
#include "handle.hh"
#include "overload.hh"
#include "types.hh"
#include <thread>
#include <unordered_set>

using namespace std;

shared_ptr<ReadOnlyRT> ReadOnlyRT::init()
{
  return std::make_shared<ReadOnlyRT>();
}

shared_ptr<ReadWriteRT> ReadWriteRT::init()
{
  return std::make_shared<ReadWriteRT>();
}

Handle<Value> ReadOnlyRT::execute( Handle<Relation> x )
{
  return relater_.execute( x );
}

Handle<Value> ReadWriteRT::execute( Handle<Relation> x )
{
  auto res = relater_.execute( x );

  relater_.visit_full( x, [this]( Handle<AnyDataType> h ) {
    h.visit<void>( overload {
      []( Handle<Literal> ) {},
      [&]( auto handle ) {
        auto& repo = relater_.get_repository();
        if ( not repo.contains( handle ) )
          repo.put( handle, relater_.get( handle ).value() );
      },
    } );
  } );
  return res;
}

shared_ptr<Client> Client::init( const Address& address )
{
  auto runtime = make_shared<Client>();
  runtime->network_worker_.emplace( runtime->relater_ );
  runtime->network_worker_->start();
  runtime->network_worker_->connect( address );
  runtime->server_ = runtime->network_worker_->get_remote( address );
  return runtime;
}

Client::~Client()
{
  network_worker_->stop();
}

Server::~Server()
{
  network_worker_->stop();
}

void Server::join()
{
  network_worker_->join();
}

shared_ptr<Server> Server::init( const Address& address,
                                 shared_ptr<Scheduler> scheduler,
                                 vector<Address> peer_servers )
{
  auto runtime = std::make_shared<Server>( scheduler );
  runtime->network_worker_.emplace( runtime->relater_ );
  runtime->network_worker_->start();
  runtime->network_worker_->start_server( address );

  for ( const auto& p : peer_servers ) {
    if ( p == address ) {
      break;
    }
    this_thread::sleep_for( 100ms );
    runtime->network_worker_->connect( p );
  }

  return runtime;
}

template<FixType T>
void Client::send_job( Handle<T> handle, unordered_set<Handle<Fix>> visited )
{
  if ( visited.contains( handle ) )
    return;
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if constexpr ( std::same_as<T, BlobRef> ) {
      std::visit( [&]( const auto x ) { send_job( x, visited ); }, handle.get() );
    }
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
    std::visit( [&]( const auto x ) { send_job( x, visited ); }, handle.get() );
  } else {
    if ( server_->contains( handle ) ) {
      // Load the data on the server side
      server_->put( handle, {} );
    } else {
      if constexpr ( FixTreeType<T> ) {
        if ( contains( handle ) ) {
          auto tree = get( handle ).value();
          for ( const auto& element : tree->span() ) {
            send_job( element, visited );
          }
        }
      }
      server_->put( handle, relater_.get( handle ).value() );
    }
    visited.insert( handle );
  }
}

Handle<Value> Client::execute( Handle<Relation> x )
{
  send_job( x );
  relater_.visit_full( x, [&]( Handle<AnyDataType> h ) {
    h.visit<void>( overload { []( Handle<Literal> ) {},
                              []( Handle<Relation> ) {},
                              [&]( auto h ) {
                                if ( relater_.contains( h ) ) {
                                  server_->put( h, relater_.get( h ).value() );
                                }
                              } } );
  } );

  return relater_.execute( x );
}
