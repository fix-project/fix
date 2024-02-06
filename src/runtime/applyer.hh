#pragma once

#include "channel.hh"
#include "runner.hh"
#include "types.hh"

struct ApplyEnv
{
  Handle<ObjectTree> handle { Handle<ObjectTree>::nil() };
  Runner::BlobMap minrepo_blobs {};
  Runner::TreeMap minrepo_trees {};
};

struct ApplyResult
{
  Handle<ObjectTree> handle { Handle<ObjectTree>::nil() };
  Handle<Object> result { Handle<ObjectTree>::nil() };
  Runner::BlobMap minrepo_blobs {};
  Runner::TreeMap minrepo_trees {};
};

class Applyer : public Runner
{
private:
  std::vector<std::thread> threads_ {};
  Channel<ApplyEnv> todo_ {};
  Channel<ApplyResult> result_ {};
  std::shared_ptr<Runner> runner_ {};

private:
  void run();
  void progress( ApplyEnv&& apply_todo );

public:
  virtual void init() override;
  virtual std::optional<Handle<Object>> apply( Handle<ObjectTree> handle,
                                               BlobMap&& minrepo_blobs,
                                               TreeMap&& minrepo_trees ) override;

  Applyer( size_t threads, std::shared_ptr<Runner> runner );
};
