#pragma once

#include "executor.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "repository.hh"
#include "storage_exception.hh"
#include <memory>

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
    runtime->repository_.emplace();
    runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
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

  virtual bool contains( const std::string_view label ) override
  {
    return repository_->contains( label ) || executor_->contains( label );
  }

  virtual Handle<Fix> labeled( const std::string_view label ) override
  {
    if ( repository_->contains( label ) ) {
      return repository_->labeled( label );
    }

    return executor_->labeled( label );
  }
};
