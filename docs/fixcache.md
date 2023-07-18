# Fix Cache
Fix cache has two functionalities: 1) Memoize the result of completed jobs and
2) track the dependency relationship between jobs.

## Structure of Job Names
The Handles of Jobs store two list of operations: a done list and a to-do list.
In the remaining of this document, `{[done], Name, [to-do]}` will be used to
represent a Job. For example, `{[eval], T, [eval, apply]}` is saying `eval(T)`
is done, but `apply(eval(T))` and `eval(apply(eval(T)))` are to-dos.

In order to track dependency relationship between jobs, and there could be more
than one job that depends on a certain job, we also give names to dependencies.
`{[done], Name, [to-do]}_i` is the i<sup>th</sup> job that depends
on the completion of `{[done], Name, [to-do]}`. It could be the case that
`{[done], Name, [to-do]}_i` is the same as `{[done], Name, [to-do]}_j` for i $\neq$ j.

## Dependency Relationship
There are two kind of dependency relationship: continue-after and start-after.
Given two jobs: `{x, m, y}` and `{a, n, b}`, if `{x, m, y}` continue-after
`{a, n, b}`, this is saying `x(m)` is the same as `a(n)`, and once we have the
value of `a(n)`, we can proceed on `{x, m, y}` and the following step is `y(a(n))`.

e.g. `{[eval], t, [force, apply]}` continue-after `{[eval], t, []}`. If `eval(t)`
gives `p`, the next step is `force(apply(p))`.

If `{x, m, y}` start-after `{a, n, b}`, this is saying that once all jobs
that `{x, m, y}` has a start-after dependency on completed, we can proceed on
`{x, m, y}` and the following step is `y(x(m))`.

e.g. For every entry `e` of Tree `t`, `{[], t, [fill]}` start-after `{[eval], e, []}`.
After each entry is evaled, we can start filling out the tree.

It should be the case that a job only has one of the two kinds of dependency on other jobs.

## Structure of Fix Cache Entries
Each entry of the fix cache contains three fields: `{Name: m256i, Value: m256i, pending: int64_t}`.

There are 3 types of entries:
1. `{Name: job, Value: v, pending: p}`:
- If `p = -1`, `done(job)` is completed and `v = done(job)`.
- If `p = 1`, `done(job)` is either currently being processed by a thread, or pending on
  some dependency. `v = job`.
- If `p = -2`, `done(job)` needs to be processed, by no thread is processing it
  yet. `v = job`.
- `-2, -1, 1` are the only possible values of `p` for this type of entry.

In the current implementation, if `{[force, apply, eval], t, []}` is needed, only 
`{[eval], t, []}`, `{[apply], eval(t), []}`, `{[force], apply(eval(t)), []}` and
`{[force, apply, eval], t, []}` would have an entry in this format in the
cache. In other words, for any name of jobs that could appear in the `Name`
field of this kind of entry, the `to-do` list is empty.

2. `{Name: job_i, Value: v, pending: p}`:
- If `p = 1`, v has a start-after dependency on job.
- If `p = 0`, v has a continue-after dependency on job.

3. `{Name: job, Value: arg, pending: p}`:
`job` has start-after dependencies on other jobs, and `p` is the number of
unresolved dependencies. If `p = 0`, all dependencies are resolved, and the
next step would be `to-do(arg)`.

### Canonical Name Entries
Mappings from canonical to local names are stored in runtimestorage in a similar 
format to entries in fixcache.
`{Name: canonical_name, Value: local_name}`:
These entries are not used for synchronizing jobs, but instead map canonical
names to local ids. This is mainly used by `get_blob` and `get_tree`.

## What is needed for things to not go bad
1. Jobs are done *correctly*: We should only decide `a(b(x))` is equivalent to
   `a(y)` only if we know that `b(x)` is equivalent to `y`, and a job is started
   only if all its dependencies has been resolved.

2. Jobs *make progress*: If all dependencies of a job is resolved, the job will
   be run eventually.

## What the current implementation is doing
The current implementation implies the conditions in the last section, but the
conditions in the last section does not imply the current implementation.
1. Each job is only started once by one thread (in `fixcache::try_run()`).
2. Each job is only changed to COMPLETE status once by one thread (in
   `fixcache::change_status_to_completed()`).
3. THE thread that changes a job to COMPLETE status resolves all dependencies on
   the job (in `fixcache::update_pending_jobs()`).
* For any continue-after dependency on the completed job, the thread queues the
  job directly.
* For any start-after dependency, the thread deducts the pending entry by 1 and
  the thread that changes the pending entry from 1 to 0 queues the job.
4. Dependencies on a job can only be added before the job is changed to COMPLETE
   status (in `fixcache::start_after()` and `fixcache::continue_after()`).

2 and 3 implies the "correctness" requirement. 3 and 4 implies the
"make-progress" requirement.

## To do 
An interesting question rising from the current implementation is that how fixed
points should be handled. Given a thunk `t`, `{[eval], t, []}` 
continue-after `{[eval, force], t, []}`, `{[force], t, [eval]}` continue-after 
`{[force], t, []}`, and `{[eval, force], t, []}` continue-after `{[eval], force(t), []}`.
However, if `force(t)` is `t`, this is a fixed point. Because of item 1 in the
last section, the current implementation halts after processing `eval(t)` and
`force(t)`. Is the right way of handling this?
