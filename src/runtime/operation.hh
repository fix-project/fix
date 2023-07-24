#pragma once

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <utility>

/**
 * A basic operation, which can be applied to an ::Object (via a Handle) by constructing and scheduling a Task.
 *
 * A good rule of thumb is that a certain "thing" should be an ::Operation if it ought to be either cacheable (see
 * ::FixCache) or schedulable (see RuntimeWorker).  If neither of these are true (e.g., if the "thing" is trivial),
 * it might be better to make it an implicit step in RuntimeWorker.
 */
enum class Operation : uint8_t
{
  /// Apply an ENCODE (blocking until execution is complete)
  Apply,
  /// Evaluate an Object (non-blocking)
  Eval,
};
