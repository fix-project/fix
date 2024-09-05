#pragma once

#include "handle.hh"
#include "network.hh"
#include "relater.hh"
#include "repository.hh"

#include <memory>

class FrontendRT
{
public:
  virtual ~FrontendRT() {}
  virtual Handle<Value> execute( Handle<Relation> ) = 0;
};

class ReadOnlyRT : public FrontendRT
{
protected:
  Relater relater_ {};

public:
  ReadOnlyRT() {}
  static std::shared_ptr<ReadOnlyRT> init();
  virtual Handle<Value> execute( Handle<Relation> x ) override;
  IRuntime& get_rt() { return relater_; }
};

class ReadWriteRT : public ReadOnlyRT
{
public:
  ReadWriteRT() {}

  static std::shared_ptr<ReadWriteRT> init();
  virtual Handle<Value> execute( Handle<Relation> x ) override;
};

class Client : public FrontendRT
{
protected:
  Relater relater_ { 0 };
  std::optional<NetworkWorker> network_worker_ {};
  std::shared_ptr<IRuntime> server_ {};

  template<FixType T>
  void send_job( Handle<T> handle, std::unordered_set<Handle<Fix>> visited = {} );

public:
  Client() {}
  ~Client();

  static std::shared_ptr<Client> init( const Address& address );
  virtual Handle<Value> execute( Handle<Relation> x ) override;

  IRuntime& get_rt() { return relater_; }
  std::shared_ptr<IRuntime>& get_server() { return server_; }
};

class Server
{
protected:
  Relater relater_;
  std::optional<NetworkWorker> network_worker_ {};

public:
  Server( std::shared_ptr<Scheduler> scheduler )
    : relater_( std::thread::hardware_concurrency() - 1, {}, scheduler )
  {}

  static std::shared_ptr<Server> init( const Address& address,
                                       std::shared_ptr<Scheduler> scheduler,
                                       const std::vector<Address> peer_servers = {} );
  void join();
  ~Server();
};
