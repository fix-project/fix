#pragma once

#include "task.hh"

class ITaskRunner
{
public:
  struct Info
  {
    uint32_t parallelism;
  };

  virtual std::optional<Handle> start( Task&& task ) = 0;
  virtual std::optional<Info> get_info() = 0;
  virtual ~ITaskRunner() {};
};

class IResultCache
{
public:
  virtual void finish( Task&& task, Handle result ) = 0;
  virtual ~IResultCache() {};
};

class IRuntime
  : public ITaskRunner
  , public IResultCache
{
public:
  virtual void add_task_runner( ITaskRunner& runner ) = 0;
  virtual void add_result_cache( IResultCache& cache ) = 0;
  virtual ~IRuntime() {};
};
