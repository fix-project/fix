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
  std::shared_ptr<IRuntime> local_;

  // Function to be executed on every piece of the dependency graph. Its output only dependends on the input job
  // itself.
  virtual void all( Handle<Dependee> ) = 0;
  // Function to be executed on every leaf of the dependency graph.
  virtual void data( Handle<Dependee> ) = 0;
  // Function to be executed on every depender before recurse into its dependees
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) = 0;
  // Function to be executed on every depender after recurse into its dependees
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) = 0;

public:
  Pass( std::reference_wrapper<Relater> relater );
  void run( Handle<Dependee> );
  virtual ~Pass() {}
};

class BasePass : public Pass
{
private:
  struct TaskInfo
  {
    std::unordered_map<std::shared_ptr<IRuntime>, size_t> present_size {};
    std::unordered_set<std::shared_ptr<IRuntime>> contains {};
    size_t output_size {};
    size_t output_fan_out {};
    bool ep { false };
  };

  absl::flat_hash_map<Handle<Dependee>, TaskInfo> tasks_info_ {};

  virtual void data( Handle<Dependee> ) override;
  virtual void all( Handle<Dependee> ) override;
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}

  // Calculate absent size from a root
  size_t absent_size( std::shared_ptr<IRuntime> worker, Handle<Dependee> job );

  std::vector<std::shared_ptr<IRuntime>> available_remotes_ {};

public:
  BasePass( std::reference_wrapper<Relater> relater );

  const std::unordered_map<std::shared_ptr<IRuntime>, size_t>& get_present_size( const Handle<Dependee> task ) const
  {
    return tasks_info_.at( task ).present_size;
  }

  const std::unordered_set<std::shared_ptr<IRuntime>>& get_contains( const Handle<Dependee> task ) const
  {
    return tasks_info_.at( task ).contains;
  }

  size_t get_output_size( const Handle<Dependee> task ) const { return tasks_info_.at( task ).output_size; }

  size_t get_fan_out( const Handle<Dependee> task ) const { return tasks_info_.at( task ).output_fan_out; }

  bool get_ep( const Handle<Dependee> task ) const { return tasks_info_.at( task ).ep; }

  const std::vector<std::shared_ptr<IRuntime>>& get_available_remotes() const { return available_remotes_; }
};

class SelectionPass : public Pass
{
protected:
  std::reference_wrapper<BasePass> base_;
  absl::flat_hash_map<Handle<Dependee>, std::pair<std::shared_ptr<IRuntime>, size_t>> chosen_remotes_;

public:
  SelectionPass( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater );

  SelectionPass( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 std::unique_ptr<SelectionPass> prev );

  absl::flat_hash_map<Handle<Dependee>, std::pair<std::shared_ptr<IRuntime>, size_t>>&& release()
  {
    return std::move( chosen_remotes_ );
  };

  friend class PassRunner;
};

// Only operates on local paths, except that `independent` are always invoked on root job
class PrunedSelectionPass : public SelectionPass
{
public:
  PrunedSelectionPass( std::reference_wrapper<BasePass> base,
                       std::reference_wrapper<Relater> relater,
                       std::unique_ptr<SelectionPass> prev );

  void run( Handle<Dependee> );
};

class MinAbsentMaxParallelism : public SelectionPass
{
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void data( Handle<Dependee> ) override {};
  virtual void all( Handle<Dependee> ) override {};
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}

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
  bool double_check_ { false };

  absl::flat_hash_map<Handle<Dependee>, absl::flat_hash_set<Handle<Relation>>> dependees_ {};

  virtual void all( Handle<Dependee> ) override;
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void data( Handle<Dependee> ) override {}
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {};

public:
  ChildBackProp( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}

  ChildBackProp( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 std::unique_ptr<SelectionPass> prev )
    : SelectionPass( base, relater, move( prev ) )
  {}

  void run( Handle<Dependee> );
};

class InOutSource : public PrunedSelectionPass
{
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void data( Handle<Dependee> ) override {}
  virtual void all( Handle<Dependee> ) override {}
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}

public:
  InOutSource( std::reference_wrapper<BasePass> base,
               std::reference_wrapper<Relater> relater,
               std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
  {}
};

class RandomSelection : public SelectionPass
{
  virtual void data( Handle<Dependee> ) override;
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void all( Handle<Dependee> ) override {}
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}

  std::unordered_set<Handle<Dependee>> applys_ {};
  absl::flat_hash_map<Handle<Dependee>, bool> recursively_depend_on_apply_ {};

public:
  RandomSelection( std::reference_wrapper<BasePass> base,
                   std::reference_wrapper<Relater> relater,
                   std::unique_ptr<SelectionPass> prev )
    : SelectionPass( base, relater, move( prev ) )
  {}

  const std::unordered_set<Handle<Dependee>> get_applys() const { return applys_; }
};

class SendToRemotePass : public PrunedSelectionPass
{
  std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<Dependee>>> remote_jobs_ {};
  std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<Dependee>>> remote_data_ {};
  void send_job_dependencies( std::shared_ptr<IRuntime>, Handle<Dependee> );

  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;
  virtual void all( Handle<Dependee> ) override;

  virtual void data( Handle<Dependee> ) override {};
  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}

public:
  SendToRemotePass( std::reference_wrapper<BasePass> base,
                    std::reference_wrapper<Relater> relater,
                    std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
  {}

  void send_remote_jobs( Handle<Dependee> );
};

class FinalPass : public PrunedSelectionPass
{
  std::optional<Handle<Relation>> todo_ {};
  std::reference_wrapper<SketchGraphScheduler> sch_;

  virtual void data( Handle<Dependee> ) override;
  virtual void relation_pre( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override;

  virtual void relation_post( Handle<Relation>, const absl::flat_hash_set<Handle<Dependee>>& ) override {}
  virtual void all( Handle<Dependee> ) override {};

public:
  FinalPass( std::reference_wrapper<BasePass> base,
             std::reference_wrapper<Relater> relater,
             std::reference_wrapper<SketchGraphScheduler> sch,
             std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
    , sch_( sch )
  {}

  std::optional<Handle<Relation>> get_todo() { return todo_; };
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

  static std::optional<Handle<Thunk>> run( std::reference_wrapper<Relater> rt,
                                           std::reference_wrapper<SketchGraphScheduler> sch,
                                           Handle<Dependee> top_level_job,
                                           const std::vector<PassType>& passes );

  static std::optional<Handle<Thunk>> random_run( std::reference_wrapper<Relater> rt,
                                                  std::reference_wrapper<SketchGraphScheduler> sch,
                                                  Handle<Dependee> top_level_job,
                                                  const std::vector<PassType>& passes );
};
