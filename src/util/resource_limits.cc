#include "resource_limits.hh"

namespace resource_limits {
thread_local uint64_t available_bytes = 0;
};
