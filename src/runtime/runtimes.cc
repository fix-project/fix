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
                                 vector<Address> peer_servers,
                                 optional<size_t> threads )
{
  auto runtime = std::make_shared<Server>( scheduler, threads );
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
    std::visit( [&]( auto x ) { send_job( x, visited ); }, handle.get() );
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
    if ( relater_.contains( handle ).has_value() ) {
      std::visit( [&]( auto x ) { send_job( x, visited ); }, relater_.contains( handle ).value().get() );
    }
  } else {
    if constexpr ( std::same_as<T, Literal> ) {
      return;
    } else {
      if ( server_->contains( handle ) ) {
        // Load the data on the server side
        VLOG( 3 ) << "send_job loading " << handle << " on server";
        server_->put( handle, {} );
      } else {
        VLOG( 3 ) << "send_job sending " << handle << " on server";
        if constexpr ( FixTreeType<T> ) {
          if ( relater_.contains( handle ) ) {
            auto tree = relater_.get( handle ).value();
            for ( const auto& element : tree->span() ) {
              send_job( element, visited );
            }
          }
        }

        if ( relater_.contains( handle ) ) {
          server_->put( handle, relater_.get( handle ).value() );
        }
      }
      visited.insert( handle );
    }
  }
}

Handle<Value> Client::execute( Handle<Relation> x )
{
  send_job( x );
  return relater_.execute( x );
}
