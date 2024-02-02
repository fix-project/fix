#pragma once

#include "executor.hh"
#include "handle.hh"
#include "network.hh"
#include "repository.hh"

#include <memory>

class FrontendRT : public IRuntime
{
  virtual Handle<Value> execute( Handle<Relation> ) = 0;
};

class ReadOnlyRT : public FrontendRT
{
protected:
  std::optional<Executor> executor_ {};
  std::optional<Repository> repository_ {};

public:
  ReadOnlyRT() {}

  static std::shared_ptr<ReadOnlyRT> init();
  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual bool contains( const std::string_view label ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

  template<typename T, typename S>
  std::optional<T> get( Handle<S> name );
  template<typename T, typename S>
  void put( Handle<S> name, T data );
  template<typename T>
  bool contains( T name );

  virtual Handle<Value> execute( Handle<Relation> x ) override;
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
  std::optional<Repository> repository_ {};
  std::optional<NetworkWorker> network_worker_ {};
  std::shared_ptr<Remote> remote_ {};
  std::optional<Executor> executor_ {};

public:
  Client() {}

  template<FixType T>
  void send( Handle<T> handle );

  static std::shared_ptr<Client> init( const Address& address );

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual bool contains( const std::string_view label ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

  template<typename T, typename S>
  std::optional<T> get( Handle<S> name );
  template<typename T, typename S>
  void put( Handle<S> name, T data );
  template<typename T>
  bool contains( T name );

  virtual Handle<Value> execute( Handle<Relation> x ) override;

  ~Client();
};

class Server : public IRuntime
{
protected:
  std::optional<Repository> repository_ {};
  std::optional<NetworkWorker> network_worker_ {};
  std::optional<Executor> executor_ {};

public:
  Server() {}

  static std::shared_ptr<Server> init( const Address& address );

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual bool contains( const std::string_view label ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

  template<typename T, typename S>
  std::optional<T> get( Handle<S> name );
  template<typename T, typename S>
  void put( Handle<S> name, T data );
  template<typename T>
  bool contains( T name );

  ~Server();
};
