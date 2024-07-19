#pragma once

#include <atomic>
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
  Channel<Handle<AnyDataType>> todo_ {};
  Relater& parent_;
  std::shared_ptr<Runner> runner_ {};

  std::atomic<bool> top_level_done_ { false };
  Handle<Relation> top_level {};
  Handle<Value> result {};

public:
  Executor( Relater& parent,
            size_t threads = std::thread::hardware_concurrency(),
            std::optional<std::shared_ptr<Runner>> runner = {} );

  ~Executor();

  Handle<Value> execute( Handle<Relation> x )
  {
    {
      if ( parent_.contains( x ) ) {
        VLOG( 2 ) << "Relation existed " << x;
        return parent_.get( x )->unwrap<Value>();
      }
    }

    VLOG( 1 ) << "Relation does not exit " << x.content;

    top_level = x;
    top_level_done_ = false;
    todo_.move_push( x );

    top_level_done_.wait( false, std::memory_order_acquire );
    return result;
  }

  bool finish( Handle<Relation> name, Handle<Object> value )
  {
    if ( name == top_level ) {
      result = value.unwrap<Value>();
      top_level_done_.store( true, std::memory_order_release );
      top_level_done_.notify_all();

      return true;
    }

    return false;
  }

private:
  template<typename T>
  using Result = FixEvaluator::Result<T>;

  void run();
  void progress( Handle<AnyDataType> runnable_or_loadable );

  /** @defgroup Implementation of FixRuntime
   * @{
   */

  std::optional<Handle<Fix>> load( Handle<AnyDataType> handle );
  Result<Object> apply( Handle<ObjectTree> combination );

public:
  /** @defgroup Implementation of IRuntime
   * @{
   */

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
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
