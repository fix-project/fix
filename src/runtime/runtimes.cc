#include "runtimes.hh"
#include "handle.hh"
#include "overload.hh"
#include "types.hh"

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
    h.visit<void>( overload { []( Handle<Literal> ) {},
                              [&]( auto handle ) {
                                auto repo = relater_.get_repository();
                                if ( not repo.contains( handle ) )
                                  repo.put( handle, relater_.get( handle ).value() );
                              } } );
  } );
  return res;
}

shared_ptr<Client> Client::init( const Address& address )
{
  auto runtime = make_shared<Client>();
  runtime->network_worker_.emplace( runtime->relater_ );
  runtime->network_worker_->start();
  runtime->network_worker_->connect( address );
  runtime->network_worker_->get_remote( address );
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

shared_ptr<Server> Server::init( const Address& address, vector<Address> peer_servers )
{
  auto runtime = std::make_shared<Server>();
  runtime->network_worker_.emplace( runtime->relater_ );
  runtime->network_worker_->start();

  for ( const auto& p : peer_servers ) {
    if ( p == address ) {
      break;
    }
    runtime->network_worker_->connect( p );
  }
  runtime->network_worker_->start_server( address );
  
  return runtime;
}

Handle<Value> Client::execute( Handle<Relation> x )
{
  return relater_.execute( x );
}
