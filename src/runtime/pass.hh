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
    size_t output_size {};
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

  const std::unordered_set<std::shared_ptr<IRuntime>>& get_contains( const Handle<AnyDataType> task ) const
  {
    return tasks_info_.at( task ).contains;
  }

  size_t get_output_size( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).output_size; }

  size_t get_fan_out( const Handle<AnyDataType> task ) const { return tasks_info_.at( task ).output_fan_out; }
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
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

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
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

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

class SendtoRemote : public SelectionPass
{
  std::shared_ptr<IRuntime> target_remote_;
  absl::flat_hash_set<Handle<AnyDataType>>& to_send_;

  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {};
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {};

  virtual void leaf( Handle<AnyDataType> ) override;

public:
  SendtoRemote( std::reference_wrapper<BasePass> base,
                std::reference_wrapper<Relater> relater,
                std::shared_ptr<IRuntime> target_remote,
                absl::flat_hash_set<Handle<AnyDataType>>& to_send )
    : SelectionPass( base, relater )
    , target_remote_( target_remote )
    , to_send_( to_send )
  {}
};

class OutSource : public PrunedSelectionPass
{
  std::map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>>& to_sends_;

  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;

  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void independent( Handle<AnyDataType> ) override {};
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  OutSource( std::reference_wrapper<BasePass> base,
             std::reference_wrapper<Relater> relater,
             std::unique_ptr<SelectionPass> prev,
             std::map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>>& to_sends )
    : PrunedSelectionPass( base, relater, move( prev ) )
    , to_sends_( to_sends )
  {}
};

class RandomSelection : public SelectionPass
{
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}
  virtual void leaf( Handle<AnyDataType> ) override {}
  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

  virtual void independent( Handle<AnyDataType> ) override;

public:
  RandomSelection( std::reference_wrapper<BasePass> base, std::reference_wrapper<Relater> relater )
    : SelectionPass( base, relater )
  {}
};

class FinalPass : public PrunedSelectionPass
{
  std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>> remote_jobs_ {};

  virtual void leaf( Handle<AnyDataType> ) override;
  virtual void pre( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override;
  virtual void independent( Handle<AnyDataType> ) override;

  virtual void post( Handle<AnyDataType>, const absl::flat_hash_set<Handle<AnyDataType>>& ) override {}

public:
  FinalPass( std::reference_wrapper<BasePass> base,
             std::reference_wrapper<Relater> relater,
             std::unique_ptr<SelectionPass> prev )
    : PrunedSelectionPass( base, relater, move( prev ) )
  {}

  const std::unordered_map<std::shared_ptr<IRuntime>, absl::flat_hash_set<Handle<AnyDataType>>>& get_remote_jobs()
  {
    return remote_jobs_;
  };

  void make_root_local( Handle<AnyDataType> );
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
    OutSource,
    Random
  };

  static void run( std::reference_wrapper<Relater> rt,
                   Handle<AnyDataType> top_level_job,
                   std::vector<PassType> passes );
};
