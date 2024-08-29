#include "runtimes.hh"
#include "handle.hh"
#include "overload.hh"
#include "types.hh"
#include <thread>

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

Handle<Value> Client::execute( Handle<Relation> x )
{
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
