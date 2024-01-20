#pragma once
#include "executor.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "repository.hh"
#include "storage_exception.hh"
#include <memory>

#pragma GCC diagnostic ignored "-Wunused-function"

class ReadOnlyTester : public IRuntime
{
  std::optional<Executor> executor_ {};
  std::optional<Repository> repository_ {};

public:
  ReadOnlyTester() {}
  Executor& executor() { return *executor_; }

  static std::shared_ptr<ReadOnlyTester> init()
  {
    auto runtime = std::make_shared<ReadOnlyTester>();
    runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
    runtime->repository_.emplace();
    return runtime;
  }

  virtual std::optional<BlobData> get( Handle<Named> name ) override
  {
    if ( executor_->contains( name ) ) {
      return executor_->get( name );
    } else if ( repository_->contains( name ) ) {
      auto blob = repository_->get( name );
      executor_->put( name, *blob );
      return *blob;
    }

    throw HandleNotFound( name );
  }

  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override
  {
    if ( executor_->contains( name ) ) {
      return executor_->get( name );
    } else if ( repository_->contains( name ) ) {
      auto tree = repository_->get( name );
      executor_->put( name, *tree );
      return *tree;
    }

    throw HandleNotFound( handle::fix( name ) );
  };

  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override
  {
    if ( repository_->contains( name ) ) {
      auto relation = repository_->get( name );
      repository_->put( name, *relation );
      return *relation;
    } else {
      return executor_->get( name );
    }
  };

  virtual void put( Handle<Named> name, BlobData data ) override { executor_->put( name, data ); };

  virtual void put( Handle<AnyTree> name, TreeData data ) override { executor_->put( name, data ); }

  virtual void put( Handle<Relation> name, Handle<Object> data ) override { executor_->put( name, data ); }

  virtual bool contains( Handle<Named> handle ) override
  {
    return executor_->contains( handle ) || repository_->contains( handle );
  }

  virtual bool contains( Handle<AnyTree> handle ) override
  {
    return executor_->contains( handle ) || repository_->contains( handle );
  }

  virtual bool contains( Handle<Relation> handle ) override
  {
    return executor_->contains( handle ) || repository_->contains( handle );
  }

  virtual Handle<Fix> labeled( const std::string_view label ) override { return repository_->labeled( label ); };
};

static Handle<Strict> compile( ReadOnlyTester& rt, Handle<Fix> wasm )
{
  auto compiler = rt.labeled( "compile-encode" );

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = Handle<Literal>( "unused" );
  tree.at( 1 ) = compiler;
  tree.at( 2 ) = wasm;
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Strict>>( []( auto x ) {
    return Handle<Strict>( Handle<Application>( Handle<ExpressionTree>( x ) ) );
  } );
}

template<FixHandle... Args>
Handle<AnyTree> tree( ReadOnlyTester& rt, Args... args )
{
  OwnedMutTree tree = OwnedMutTree::allocate( sizeof...( args ) );
  size_t i = 0;
  (void)i;
  (
    [&] {
      tree[i] = args;
      i++;
    }(),
    ... );

  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) );
}

static Handle<Blob> blob( ReadOnlyTester& rt, std::string_view contents )
{
  auto blob = OwnedMutBlob::allocate( contents.size() );
  memcpy( blob.data(), contents.data(), contents.size() );
  return rt.create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
}

static Handle<Blob> file( ReadOnlyTester& rt, std::filesystem::path path )
{
  return rt.create( std::make_shared<OwnedBlob>( path ) );
}
