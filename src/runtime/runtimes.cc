#include "runtimes.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include "types.hh"

using namespace std;

template<FixType T>
void get_from_repository( Handle<T> handle, Repository& rp, IRuntime& rt )
{
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if constexpr ( std::same_as<T, Encode> ) {
      Handle<Thunk> thunk
        = handle.template visit<Handle<Thunk>>( []( auto s ) { return s.template unwrap<Thunk>(); } );
      thunk.visit<void>(
        overload { [&]( Handle<Application> a ) { get_from_repository( a.unwrap<ExpressionTree>(), rp, rt ); },
                   []( auto ) {} } );
    }

    if constexpr ( std::same_as<T, Relation> ) {
      auto rhs = rp.get( handle ).value();
      get_from_repository( rhs, rp, rt );
      rt.put( handle, rp.get( handle ).value() );
      return;
    }

    if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> or std::same_as<T, ValueTreeRef>
                        or std::same_as<T, ObjectTreeRef> ) )
      std::visit( [&]( const auto x ) { get_from_repository( x, rp, rt ); }, handle.get() );

  } else {
    if constexpr ( FixTreeType<T> ) {
      // Having the handle means that the data presents in storage
      if ( rt.contains( handle ) )
        return;

      auto tree = rp.get( handle );
      for ( const auto& element : tree.value()->span() ) {
        get_from_repository( element, rp, rt );
      }
      rt.put( handle, tree.value() );
    } else {
      auto named = handle::extract<Named>( handle ).value();
      rt.put( named, rp.get( named ).value() );
    }
  }
}

shared_ptr<ReadOnlyRT> ReadOnlyRT::init()
{
  auto runtime = std::make_shared<ReadOnlyRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), *runtime );
  return runtime;
}

shared_ptr<ReadWriteRT> ReadWriteRT::init()
{
  auto runtime = std::make_shared<ReadWriteRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), *runtime );
  return runtime;
}

template<typename T, typename S>
optional<T> ReadOnlyRT::get( Handle<S> name )
{
  if ( not executor_->contains( name ) and repository_->contains( name ) ) {
    get_from_repository( name, *repository_, *executor_ );
  }
  return executor_->get( name );
}

optional<BlobData> ReadOnlyRT::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> ReadOnlyRT::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> ReadOnlyRT::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void ReadOnlyRT::put( Handle<S> name, T data )
{
  return executor_->put( name, data );
}

void ReadOnlyRT::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void ReadOnlyRT::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void ReadOnlyRT::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool ReadOnlyRT::contains( T name )
{
  return executor_->contains( name ) || repository_->contains( name );
}

bool ReadOnlyRT::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool ReadOnlyRT::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool ReadOnlyRT::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool ReadOnlyRT::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> ReadOnlyRT::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}

Handle<Value> ReadOnlyRT::execute( Handle<Relation> x )
{
  return executor_->execute( x );
}

Handle<Value> ReadWriteRT::execute( Handle<Relation> x )
{
  if ( contains( x ) ) {
    return get( x ).value().unwrap<Value>();
  }

  auto res = executor_->execute( x );

  executor_->visit( x, [this]( Handle<AnyDataType> h ) {
    h.visit<void>(
      overload { []( Handle<Literal> ) {},
                 [&]( auto handle ) { repository_->put( handle, executor_->get( handle ).value() ); } } );
  } );
  return res;
}

shared_ptr<Client> Client::init( const Address& address )
{
  auto runtime = make_shared<Client>();
  runtime->repository_.emplace();
  runtime->network_worker_.emplace( runtime );
  runtime->network_worker_->start();
  runtime->network_worker_->connect( address );
  runtime->remote_ = runtime->network_worker_->get_remote( address );
  runtime->executor_.emplace( 0, *runtime );
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

shared_ptr<Server> Server::init( const Address& address )
{
  auto runtime = std::make_shared<Server>();
  runtime->repository_.emplace();
  runtime->network_worker_.emplace( runtime );
  runtime->network_worker_->start();
  runtime->network_worker_->start_server( address );
  runtime->executor_.emplace( std::thread::hardware_concurrency(), *runtime );
  return runtime;
}

Handle<Value> Client::execute( Handle<Relation> x )
{
  auto send = [&]( Handle<AnyDataType> h ) {
    h.visit<void>( overload { []( Handle<Literal> ) {},
                              []( Handle<Relation> ) {},
                              [&]( auto x ) {
                                auto remote = remote_.lock();
                                if ( remote )
                                  remote->put( x, get( x ).value() );
                              } } );
  };

  x.visit<void>( overload { [&]( Handle<Apply> x ) { executor_->visit_minrepo( x.unwrap<ObjectTree>(), send ); },
                            [&]( Handle<Eval> x ) { executor_->visit( x.unwrap<Object>(), send ); } } );

  {
    auto remote = remote_.lock();
    if ( remote ) {
      remote->get( x );
    } else {
      executor_->get( x );
    }
  }

  return executor_->execute( x );
}

template<typename T, typename S>
optional<T> Client::get( Handle<S> name )
{
  if ( executor_->contains( name ) )
    return executor_->get( name );

  if ( repository_->contains( name ) ) {
    get_from_repository( name, *repository_, *executor_ );
    return executor_->get( name );
  }

  auto remote = remote_.lock();
  if ( remote ) {
    return remote->get( name );
  } else {
    return executor_->get( name );
  }
}

optional<BlobData> Client::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> Client::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> Client::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void Client::put( Handle<S> name, T data )
{
  executor_->put( name, data );
}

void Client::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void Client::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void Client::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool Client::contains( T name )
{
  auto contain = executor_->contains( name ) || repository_->contains( name );
  auto remote = remote_.lock();
  if ( remote ) {
    contain |= remote->contains( name );
  }
  return contain;
}

bool Client::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool Client::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool Client::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool Client::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> Client::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}

template<typename T, typename S>
optional<T> Server::get( Handle<S> name )
{
  if ( not executor_->contains( name ) and repository_->contains( name ) ) {
    get_from_repository( name, *repository_, *executor_ );
  }
  return executor_->get( name );
}

optional<BlobData> Server::get( Handle<Named> name )
{
  return get<BlobData, Named>( name );
}
optional<TreeData> Server::get( Handle<AnyTree> name )
{
  return get<TreeData, AnyTree>( name );
}
optional<Handle<Object>> Server::get( Handle<Relation> name )
{
  return get<Handle<Object>, Relation>( name );
}

template<typename T, typename S>
void Server::put( Handle<S> name, T data )
{
  if ( !executor_->contains( name ) )
    executor_->put( name, data );
  for ( const auto& connection : network_worker_->connections_.read().get() ) {
    if ( !connection.second->contains( name ) )
      connection.second->put( name, data );
  }
}

void Server::put( Handle<Named> name, BlobData data )
{
  return put<BlobData, Named>( name, data );
}
void Server::put( Handle<AnyTree> name, TreeData data )
{
  return put<TreeData, AnyTree>( name, data );
}
void Server::put( Handle<Relation> name, Handle<Object> data )
{
  return put<Handle<Object>, Relation>( name, data );
}

template<typename T>
bool Server::contains( T name )
{
  return executor_->contains( name ) || repository_->contains( name );
}

bool Server::contains( Handle<Named> handle )
{
  return contains<Handle<Named>>( handle );
}
bool Server::contains( Handle<AnyTree> handle )
{
  return contains<Handle<AnyTree>>( handle );
}
bool Server::contains( Handle<Relation> handle )
{
  return contains<Handle<Relation>>( handle );
}
bool Server::contains( const std::string_view label )
{
  return contains<const std::string_view>( label );
}

Handle<Fix> Server::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }
  return executor_->labeled( label );
}
