#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>

#include <x86intrin.h>

class Timer
{
public:
  static void pp_ticks( std::ostream& out, const uint64_t duration_ticks ) { out << duration_ticks << " ticks"; }

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
    DNS,
    Nonblock,
    WaitingForEvent,
    count
  };

  constexpr static size_t num_categories = static_cast<size_t>( Category::count );

  constexpr static std::array<const char*, num_categories> _category_names {
    { "DNS", "Nonblocking operations", "Waiting for event" }
  };

private:
  uint64_t _beginning_timestamp = __rdtsc();
  std::array<Record, num_categories> _records {};
  std::optional<Category> _current_category {};
  uint64_t _start_time {};

public:
  template<Category category>
  void start( const uint64_t now = __rdtsc() )
  {
    if ( _current_category.has_value() ) {
      throw std::runtime_error( "timer started when already running" );
    }

    _current_category = category;
    _start_time = now;
  }

  template<Category category>
  void stop( const uint64_t now = __rdtsc() )
  {
    if ( not _current_category.has_value() or _current_category.value() != category ) {
      throw std::runtime_error( "timer stopped when not running, or with mismatched category" );
    }

    _records[static_cast<size_t>( category )].log( now - _start_time );
    _current_category.reset();
  }

  void summary( std::ostream& out ) const;
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

template<Timer::Category category>
class GlobalScopeTimer
{
public:
  GlobalScopeTimer() { global_timer().start<category>(); }
  ~GlobalScopeTimer() { global_timer().stop<category>(); }
};

template<Timer::Category category>
class RecordScopeTimer
{
  Timer::Record* _timer;
  uint64_t _start_time;

public:
  RecordScopeTimer( Timer::Record& timer )
    : _timer( &timer )
    , _start_time( __rdtsc() )
  {
    global_timer().start<category>( _start_time );
  }

  ~RecordScopeTimer()
  {
    const uint64_t now = __rdtsc();
    _timer->log( now - _start_time );
    global_timer().stop<category>( now );
  }

  RecordScopeTimer( const RecordScopeTimer& ) = delete;
  RecordScopeTimer& operator=( const RecordScopeTimer& ) = delete;
};
