#pragma once

#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "channel.hh"
#include "evaluator.hh"
#include "handle.hh"
#include "interface.hh"
#include "relater.hh"
#include "runner.hh"

class Executor : public IRuntime
{
  std::vector<std::thread> threads_ {};
  Channel<Handle<Relation>> todo_ {};
  Relater& parent_;
  std::shared_ptr<Runner> runner_ {};

public:
  Executor( Relater& parent,
            size_t threads = std::thread::hardware_concurrency(),
            std::optional<std::shared_ptr<Runner>> runner = {} );

  ~Executor();

private:
  template<typename T>
  using Result = FixEvaluator::Result<T>;

  void run();
  void progress( Handle<Relation> runnable );

public:
  Result<Object> apply( Handle<ObjectTree> combination );

  /** @defgroup Implementation of IRuntime
   * @{
   */
  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual std::optional<Handle<AnyTree>> get_handle( Handle<AnyTree> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;
  virtual bool contains( const std::string_view label ) override;

  /* }@ */
  virtual std::optional<Info> get_info() override
  {
    return Info { .parallelism = static_cast<uint32_t>( threads_.size() ),
                  .link_speed = std::numeric_limits<double>::max() };
  }
};
