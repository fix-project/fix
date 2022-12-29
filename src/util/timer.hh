#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>

#include <iostream>
#include <x86intrin.h>

class Timer
{
public:
  struct Record
  {
    uint64_t count = 0;
    uint64_t total_ticks = 0;
    uint64_t max_ticks = 0;
    uint64_t min_ticks = std::numeric_limits<uint64_t>::max();

    void log( const uint64_t ticks )
    {
      count++;
      total_ticks += ticks;
      max_ticks = std::max( max_ticks, ticks );
      min_ticks = std::min( min_ticks, ticks );
    }

    void reset()
    {
      count = total_ticks = max_ticks = 0;
      min_ticks = std::numeric_limits<uint64_t>::max();
    }
  };

  enum class Category
  {
    Hash,
    Execution,
    AttachBlob,
    CreateBlob,
    AttachTree,
    CreateTree,
    Compiling,
    Linking,
    Populating,
    count
  };

  constexpr static size_t num_categories = static_cast<size_t>( Category::count );

  constexpr static std::array<const char*, num_categories> _category_names { { "Hash",
                                                                               "Execution",
                                                                               "Attach blob",
                                                                               "Create blob",
                                                                               "Attach tree",
                                                                               "Create tree",
                                                                               "Compiling Wasm to C",
                                                                               "Linking C to Elf",
                                                                               "Populating instances" } };

  static uint64_t baseline_;

private:
  uint64_t _beginning_timestamp = __rdtsc();
  std::array<Record, num_categories> _records {};
  std::optional<Category> _current_category {};
  uint64_t _start_time {};

public:
  template<Category category>
  void start()
  {
    if ( _current_category.has_value() ) {
      std::cout << static_cast<size_t>( _current_category.value() ) << std::endl;
      abort();
    }

    const uint64_t now = __rdtsc();
    _mm_lfence();

    _current_category = category;
    _start_time = now;
  }

  template<Category category>
  void stop()
  {
    if ( not _current_category.has_value() or _current_category.value() != category ) {
      abort();
    }

    _mm_lfence();
    const uint64_t now = __rdtsc();

    _records[static_cast<size_t>( category )].log( now - _start_time );
    _current_category.reset();
  }

  template<Category category>
  uint64_t mean()
  {
    size_t category_idx = static_cast<size_t>( category );
    return _records.at( category_idx ).total_ticks / _records.at( category_idx ).count;
  }

  void summary( std::ostream& out ) const;
  void average( std::ostream& out, int count ) const;
};

inline Timer& global_timer()
{
  static Timer the_global_timer;
  return the_global_timer;
}

inline void reset_global_timer()
{
  global_timer() = Timer {};
}

inline void set_time_baseline( uint64_t baseline )
{
  Timer::baseline_ = baseline;
}

#ifndef TIME_FIXPOINT

template<Timer::Category category>
class GlobalScopeTimer
{
public:
  GlobalScopeTimer() {}
  ~GlobalScopeTimer() {}
};

#elif TIME_FIXPOINT == 1
template<Timer::Category category>
class GlobalScopeTimer
{
public:
  GlobalScopeTimer() {}
  ~GlobalScopeTimer() {}
};

template<>
inline GlobalScopeTimer<Timer::Category::Execution>::GlobalScopeTimer()
{
  global_timer().start<Timer::Category::Execution>();
}

template<>
inline GlobalScopeTimer<Timer::Category::Execution>::~GlobalScopeTimer()
{
  global_timer().stop<Timer::Category::Execution>();
}

#elif TIME_FIXPOINT == 2
template<Timer::Category category>
class GlobalScopeTimer
{
public:
  GlobalScopeTimer() { global_timer().start<category>(); }
  ~GlobalScopeTimer() { global_timer().stop<category>(); }
};

template<>
inline GlobalScopeTimer<Timer::Category::Execution>::GlobalScopeTimer()
{}

template<>
inline GlobalScopeTimer<Timer::Category::Execution>::~GlobalScopeTimer()
{}

#endif
