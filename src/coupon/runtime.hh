#pragma once

#include "couponcollector.hh"
#include "handle.hh"
#include "object.hh"
#include "runner.hh"
#include "runtimestorage.hh"

struct KernelExecutionTag
{
  Handle<Blob> procedure;
  Handle<Tree> combination;
  Handle<Fix> result;
};

class DeterministicEquivRuntime
{
public:
  virtual BlobData attach( Handle<Blob> ) = 0;
  virtual TreeData attach( Handle<Treeish> ) = 0;
  virtual size_t length( Handle<Value> ) = 0;
  virtual size_t recursive_size( Handle<Treeish> ) = 0;
  virtual Handle<Blob> create_blob( BlobData ) = 0;
  virtual Handle<Tree> create_tree( TreeData ) = 0;
  virtual Handle<Tag> create_tag( OwnedMutTree&& tree ) = 0;
  virtual Handle<Thunk> create_application_thunk( Handle<Tree> ) = 0;
  virtual Handle<Thunk> create_selection_thunk( Handle<Tree> ) = 0;
  virtual Handle<Thunk> create_identification_thunk( Handle<Value> ) = 0;
  virtual Handle<Encode> create_strict_encode( Handle<Thunk> ) = 0;
  virtual Handle<Encode> create_shallow_encode( Handle<Thunk> ) = 0;

  virtual bool is_blob( Handle<Fix> ) = 0;
  virtual bool is_tree( Handle<Fix> ) = 0;
  virtual bool is_tag( Handle<Fix> ) = 0;
  virtual bool is_thunk( Handle<Fix> ) = 0;
  virtual Handle<TreeishRef> ref( Handle<Treeish> ) = 0;
  virtual Handle<BlobRef> ref( Handle<Blob> ) = 0;
  virtual Handle<Fix> ref( Handle<Fix> ) = 0;

  virtual ~DeterministicEquivRuntime() {};
};

class DeterministicRuntime : public DeterministicEquivRuntime
{
public:
  virtual u8x32 get_name( Handle<Fix> ) = 0;
  virtual bool is_encode( Handle<Fix> ) = 0;
};

class DeterministicTagRuntime : public DeterministicRuntime
{
public:
  virtual Handle<Tag> create_arbitrary_tag( TreeData ) = 0;
};

class NonDeterministicRuntime : public DeterministicRuntime
{
public:
  virtual KernelExecutionTag execute( Handle<Blob> machine_code, Handle<Tree> combination ) = 0;
  virtual Handle<Blob> load( Handle<Blob> ) = 0;
  virtual Handle<Blob> load( Handle<BlobRef> ) = 0;
  virtual Handle<Treeish> load( Handle<Treeish> ) = 0;
  virtual Handle<Treeish> load( Handle<TreeishRef> ) = 0;

  virtual Handle<Thunk> unwrap( Handle<Encode> ) = 0;
  virtual Handle<Tree> unwrap( Handle<Thunk> ) = 0;
};

class Runtime
  : public DeterministicTagRuntime
  , public NonDeterministicRuntime
{};

inline thread_local Handle<Blob> current_procedure_ {};

class TestRuntime : public Runtime
{
private:
  RuntimeStorage storage_ {};
  PointerRunner runner_ {};

public:
  virtual BlobData attach( Handle<Blob> ) override;
  virtual TreeData attach( Handle<Treeish> ) override;
  virtual size_t length( Handle<Value> ) override;
  virtual size_t recursive_size( Handle<Treeish> ) override;
  virtual Handle<Blob> create_blob( BlobData ) override;
  virtual Handle<Tree> create_tree( TreeData ) override;
  virtual Handle<Tag> create_tag( OwnedMutTree&& ) override;
  virtual Handle<Thunk> create_application_thunk( Handle<Tree> ) override;
  virtual Handle<Thunk> create_selection_thunk( Handle<Tree> ) override;
  virtual Handle<Thunk> create_identification_thunk( Handle<Value> ) override;
  virtual Handle<Encode> create_strict_encode( Handle<Thunk> ) override;
  virtual Handle<Encode> create_shallow_encode( Handle<Thunk> ) override;

  virtual bool is_blob( Handle<Fix> ) override;
  virtual bool is_tree( Handle<Fix> ) override;
  virtual bool is_tag( Handle<Fix> ) override;
  virtual bool is_thunk( Handle<Fix> ) override;
  virtual Handle<TreeishRef> ref( Handle<Treeish> ) override;
  virtual Handle<BlobRef> ref( Handle<Blob> ) override;
  virtual Handle<Fix> ref( Handle<Fix> ) override;

  virtual u8x32 get_name( Handle<Fix> ) override;
  virtual bool is_encode( Handle<Fix> ) override;

  virtual Handle<Tag> create_arbitrary_tag( TreeData ) override;

  virtual KernelExecutionTag execute( Handle<Blob> machine_code, Handle<Tree> combination ) override;
  virtual Handle<Blob> load( Handle<Blob> ) override;
  virtual Handle<Blob> load( Handle<BlobRef> ) override;
  virtual Handle<Treeish> load( Handle<Treeish> ) override;
  virtual Handle<Treeish> load( Handle<TreeishRef> ) override;

  virtual Handle<Thunk> unwrap( Handle<Encode> ) override;
  virtual Handle<Tree> unwrap( Handle<Thunk> ) override;
};
