#pragma once

#include "handle.hh"
#include "interface.hh"
#include "relater.hh"
#include <absl/container/flat_hash_set.h>
#include <functional>
#include <unordered_map>

class Pass;
class BasePass;
class SelectionPass;

inline thread_local DependencyGraph sketch_graph_;

class Pass
{
protected:
  std::reference_wrapper<Relater> relater_;

  // Function to be executed on every piece of the dependency graph. Its output only dependends on the input job
  // itself.
  virtual void independent( Handle<AnyDataType> ) = 0;
  // Function to be executed on every leaf of the dependency graph.
  virtual void leaf( Handle<AnyDataType> ) = 0;
  // Function to be executed on every depender before recurse into its dependees
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) = 0;
  // Function to be executed on every depender after recurse into its dependees
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) = 0;

public:
  Pass( std::reference_wrapper<Relater> relater );
  void run( Handle<AnyDataType> );
  virtual ~Pass() {}
};

class BasePass : public Pass
{
private:
  struct TaskInfo
  {
    std::unordered_map<std::shared_ptr<IRuntime>, size_t> absent_size {};
    std::unordered_set<std::shared_ptr<IRuntime>> contains {};
    size_t output_size {};
    size_t output_fan_out {};
    bool ep { false };
  };

  absl::flat_hash_map<Handle<AnyDataType>, TaskInfo> tasks_info_ {};

  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void independent( Handle<AnyDataType> ) override;
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

  // Calculate absent size from a root
  size_t absent_size( std::shared_ptr<IRuntime> worker, Handle<AnyDataType> job );

public:
  BasePass( std::reference_wrapper<Relater> relater );

  const std::unordered_map<std::shared_ptr<IRuntime>, size_t>& get_absent_size(
    const Handle<AnyDataType> task ) const
  {
    return tasks_info_.at( task ).absent_size;
  }

  const std::unordered_set<std::shared_ptr<IRuntime>>& get_contains( const Handle<AnyDataType> task ) const
  {
    return tasks_info_.at( task ).contains;
  }

  size_t get_output_size( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).output_size; }

  size_t get_fan_out( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).output_fan_out; }

  bool get_ep( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).ep; }
};

class SelectionPass : public Pass
{
protected:
  std::reference_wrapper<BasePass> base_;
  absl::flat_hash_map<Handle<AnyDataType>, std::pair<std::shared_ptr<IRuntime>, int64_t>> chosen_remotes_;

public:
  SelectionPass( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater );

  SelectionPass( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 std::unique_ptr<SelectionPass> prev );

  absl::flat_hash_map<Handle<AnyDataType>, std::pair<std::shared_ptr<IRuntime>, int64_t>>&& release()
  {
    return std::move( chosen_remotes_ );
  };
};

// Only operates on local paths, except that `independent` are always invoked on root job
class PrunedSelectionPass : public SelectionPass
{
public:
  PrunedSelectionPass( std::reference_wrapper<BasePass> base,
                       std::reference_wrapper<Relater> relater,
                       std::unique_ptr<SelectionPass> prev );

  void run( Handle<AnyDataType> );
};

class MinAbsentMaxParallelism : public SelectionPass
{
  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  MinAbsentMaxParallelism( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}

  MinAbsentMaxParallelism( std::reference_wrapper<BasePass> base,
                           std::reference_wrapper<Relater> relater,
                           std::unique_ptr<SelectionPass> prev )
    : SelectionPass( base, relater, move( prev ) )
  {}
};

class ChildBackProp : public SelectionPass
{
  absl::flat_hash_map<Handle<AnyDataType>, absl::flat_hash_set<Handle<AnyDataType>>> dependees_ {};

  virtual void independent( Handle<AnyDataType> ) override;
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  ChildBackProp( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}

  ChildBackProp( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 std::unique_ptr<SelectionPass> prev )
    : SelectionPass( base, relater, move( prev ) )
  {}
};

class InOutSource : public PrunedSelectionPass
{
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  InOutSource( std::reference_wrapper<BasePass> base,
               std::reference_wrapper<Relater> relater,
               std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
  {}
};

class RandomSelection : public SelectionPass
{
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}
  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

  virtual void independent( Handle<AnyDataType> ) override;

public:
  RandomSelection( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}
};

class SendToRemotePass : public PrunedSelectionPass
{
  std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>> remote_jobs_ {};
  std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>> remote_data_ {};
  void send_job_dependencies( std::shared_ptr<IRuntime>, Handle<AnyDataType> );

  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;
  virtual void independent( Handle<AnyDataType> ) override;

  virtual void leaf( Handle<AnyDataType> ) override {};
  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  SendToRemotePass( std::reference_wrapper<BasePass> base,
                    std::reference_wrapper<Relater> relater,
                    std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
  {}

  void send_remote_jobs( Handle<AnyDataType> );
};

class FinalPass : public PrunedSelectionPass
{
  std::reference_wrapper<SketchGraphScheduler> sch_;

  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void pre( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void post( Handle<Eval>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}
  virtual void independent( Handle<AnyDataType> ) override {};

public:
  FinalPass( std::reference_wrapper<BasePass> base,
             std::reference_wrapper<Relater> relater,
             std::reference_wrapper<SketchGraphScheduler> sch,
             std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
    , sch_( sch )
  {}
};

// A correct sequence of passes contains: BasePass + (n >= 1) * SelectionPass + (n >= 0) * PrunedSelectionPass +
// FinalPass
class PassRunner
{
public:
  enum class PassType : uint8_t
  {
    MinAbsentMaxParallelism,
    ChildBackProp,
    InOutSource,
    Random
  };

  static void run( std::reference_wrapper<Relater> rt,
                   std::reference_wrapper<SketchGraphScheduler> sch,
                   Handle<AnyDataType> top_level_job,
                   const std::vector<PassType>& passes );
};
