# Fix Cache
The Fix Cache is essentially the database of all the information we know about
relationships between Fix Objects.  In practice, this boils down to two major
things:
1. A memoization cache to allow computations to reuse previous intermediate
   results.
2. A dependency graph to avoid doing duplicate work.

## Structure of a Task

Fix Cache stores data in terms of "Tasks".  A Task is essentially:
```
struct Task {
    Operation operation;
    Handle handle;
};
```

This is the basic unit of schedulable work in Fix; it represents performing an
operation on an Object referred to by a Handle.  The operations which exist at
present are `Apply`, referring to the application of an Encode, and `Eval`,
referring to the evaluation of an Object.

In addition to being the smallest schedulable unit of work in Fix, a Task is
also the smallest cacheable unit.  While each Task may involve several steps,
only the non-trivial steps should be extracted into subtasks; trivial steps
should be performed directly by RuntimeWorker.

## The Memoization Cache

The memoization cache associates `Task => Handle` (i.e., a Task to its result).
It is reasonably straightforward; the only oddity is that the cache may have an
entry for a Task but not the corresponding result.  This represents the case
where a Task has been scheduled for execution but hasn't yet finished; we know
_about_ the task and don't want to schedule it a second time, but we don't know
what the result is yet.

## The Dependency Graph

The dependency graph is effectively a multimap of `Task => Task`, associating
dependencies with dependents.  This multimap is actually stored as a regular
map associating `(Task, Index) => Task`; when inserting a new dependency
relationship we use the next unused index for that dependency.

In addition, we also track the "blocked count" of each Task.  When we finish a
Task, we update the blocked count of all of its dependents; if any of them hit
zero, we reënqueue the now-unblocked tasks.

## Task Execution (RuntimeWorker)

When a Task is scheduled to be run, we may or may not have all the dependencies
already calculated.  This is an unavoidable consequence of the fact that we
need to inspect the task to discover the dependencies.  This leads to two
high-level options:
* The dependencies are available.  In this case, we can run the Task to
  completion, and return the result of the computation.
* The dependencies are not available.  In this case, we schedule the
  dependencies for execution, mark the dependency relationships in FixCache,
  and return `std::nullopt` to signal that the computation did not complete.
  FixCache will automatically re-schedule the task again once all the
  registered dependencies have been completed.

This approach is somewhat similar to async/await (or call/cc), but without the
need for dynamic memory allocation outside of FixCache's hashmaps.  We avoid
the allocation by not having true continuations; if a task doesn't complete, we
just restart it later.  However, since the intermediate values would have been
memoized by the original run, the second run can skip past all the work the
original run did.  The memoization cache therefore obviates the need for
maintaining continuation state, given that the intermediate states are
appropriately cached.

## What is needed for things to not go bad
1. Tasks are done *correctly*: We should only decide `a(b(x)) ≡ a(y)` only if
   we know that `b(x) ≡ y`.
2. Tasks *make progress*: If all dependencies of a task are resolved, the task
   will be run eventually.
3. Tasks are *unique*: Every task is "owned", either by a thread or by
   FixCache, and there's exactly one owner for every task.

## What the current implementation is doing
The current implementation implies the conditions in the last section, but the
conditions in the last section does not imply the current implementation.
1. Each task is only started by one thread, making sure there's no existing
   entry for it (in `FixCache::add_task`).  The starting thread "owns" the
   task.
2. When a task is blocked on other jobs, ownership is transferred to FixCache.
3. When a task is finished, FixCache transfers ownership of any unblocked
   dependencies back to the current thread.
