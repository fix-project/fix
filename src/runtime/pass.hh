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
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) = 0;
  // Function to be executed on every depender after recurse into its dependees
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) = 0;

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
    std::pair<size_t, size_t> in_out_size {};
    size_t output_fan_out {};
  };

  absl::flat_hash_map<Handle<AnyDataType>, TaskInfo> tasks_info_ {};

  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void independent( Handle<AnyDataType> ) override;
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

  // Calculate absent size from a root
  size_t absent_size( std::shared_ptr<IRuntime> worker, Handle<AnyDataType> job );

public:
  BasePass( std::reference_wrapper<Relater> relater );

  const std::unordered_map<std::shared_ptr<IRuntime>, size_t>& get_absent_size(
    const Handle<AnyDataType> task ) const
  {
    return tasks_info_.at( task ).absent_size;
  }

  const std::unordered_set<std::shared_ptr<IRuntime>>& get_contains( const Handle<Relation> task ) const
  {
    return tasks_info_.at( task ).contains;
  }

  std::pair<size_t, size_t> get_in_out_size( const Handle<AnyDataType> task ) const
  {
    return tasks_info_.at( task ).in_out_size;
  }

  size_t get_fan_out( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).output_fan_out; }
};

class SelectionPass : public Pass
{
protected:
  std::reference_wrapper<BasePass> base_;
  absl::flat_hash_map<Handle<AnyDataType>, std::shared_ptr<IRuntime>> chosen_remotes_;

public:
  SelectionPass( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater );

  SelectionPass( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 SelectionPass& prev );

  absl::flat_hash_map<Handle<AnyDataType>, std::shared_ptr<IRuntime>>&& release()
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
                       SelectionPass& prev );

  void run( Handle<AnyDataType> );
};

class MinAbsentMaxParallelism : public SelectionPass
{
  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  MinAbsentMaxParallelism( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}

  MinAbsentMaxParallelism( std::reference_wrapper<BasePass> base,
                           std::reference_wrapper<Relater> relater,
                           SelectionPass& prev )
    : SelectionPass( base, relater, prev )
  {}
};

class ChildBackProp : public SelectionPass
{
  absl::flat_hash_map<Handle<AnyDataType>, absl::flat_hash_set<Handle<AnyDataType>>> dependees_ {};

  virtual void independent( Handle<AnyDataType> ) override;
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  ChildBackProp( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}

  ChildBackProp( std::reference_wrapper<BasePass> base,
                 std::reference_wrapper<Relater> relater,
                 SelectionPass& prev )
    : SelectionPass( base, relater, prev )
  {}
};

class Parallelize : public PrunedSelectionPass
{
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  Parallelize( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater, SelectionPass& prev )
    : PrunedSelectionPass( base, relater, prev )
  {}
};

class FinalPass : public PrunedSelectionPass
{
  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;
  virtual void independent( Handle<AnyDataType> ) override;

  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  FinalPass( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater, SelectionPass& prev )
    : PrunedSelectionPass( base, relater, prev )
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
    ChildPackProp,
    Parallelize
  };

  static void run( std::reference_wrapper<Relater> rt,
                   Handle<AnyDataType> top_level_job,
                   std::vector<PassType> passes );
};
