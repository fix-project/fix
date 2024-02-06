#pragma once

#include "applyer.hh"
#include "executor.hh"
#include <queue>

class NoApplyExecutor : public Executor
{
  std::thread thread_ {};
  std::queue<Handle<Relation>> todo_ {};
  std::queue<Handle<Relation>> blocked_todo_ {};
  std::unordered_set<Handle<Relation>> live_ {};
  Channel<std::unique_ptr<ApplyResult>> apply_results_ {};

  std::shared_ptr<Applyer> applyer_;

public:
  NoApplyExecutor( std::shared_ptr<Runner> runner,
                   size_t threads = std::thread::hardware_concurrency(),
                   std::weak_ptr<IRuntime> parent = {} );

  ~NoApplyExecutor();

private:
  void run();
  void progress( Handle<Relation> relation );
  void progress( std::unique_ptr<ApplyResult> relation );

  Result<Object> get_or_delegate( Handle<Relation> goal );

public:
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
};
