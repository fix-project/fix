#pragma once

#include <array>
#include <chrono>
#include <iomanip>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>

constexpr double THOUSAND = 1000.0;
constexpr double MILLION = 1000000.0;
constexpr double BILLION = 1000000000.0;

class Timer
{
public:
  static inline uint64_t timestamp_ns()
  {
    static_assert( std::is_same<std::chrono::steady_clock::duration, std::chrono::nanoseconds>::value );

    return std::chrono::steady_clock::now().time_since_epoch().count();
  }

  static void pp_ns( std::ostream& out, const uint64_t duration_ns )
  {
    out << std::fixed << std::setprecision( 1 ) << std::setw( 5 ) << std::setfill( ' ' );

    if ( duration_ns < THOUSAND ) {
      out << duration_ns << " ns";
    } else if ( duration_ns < MILLION ) {
      out << duration_ns / THOUSAND << " μs";
    } else if ( duration_ns < BILLION ) {
      out << duration_ns / MILLION << " ms";
    } else {
      out << duration_ns / BILLION << " s";
    }
  }

  struct Record
  {
    uint64_t count;
    uint64_t total_ns;
    uint64_t max_ns;
    uint64_t min_ns = std::numeric_limits<uint64_t>::max();

    void log( const uint64_t time_ns )
    {
      count++;
      total_ns += time_ns;
      max_ns = std::max( max_ns, time_ns );
      min_ns = std::min( min_ns, time_ns );
    }

    void reset()
    {
      count = total_ns = max_ns = 0;
      min_ns = std::numeric_limits<uint64_t>::max();
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
  uint64_t _beginning_timestamp = timestamp_ns();
  std::array<Record, num_categories> _records {};
  std::optional<Category> _current_category {};
  uint64_t _start_time {};

public:
  template<Category category>
  void start( const uint64_t now = timestamp_ns() )
  {
    if ( _current_category.has_value() ) {
      throw std::runtime_error( "timer started when already running" );
    }

    _current_category = category;
    _start_time = now;
  }

  template<Category category>
  void stop( const uint64_t now = timestamp_ns() )
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
    , _start_time( Timer::timestamp_ns() )
  {
    global_timer().start<category>( _start_time );
  }

  ~RecordScopeTimer()
  {
    const uint64_t now = Timer::timestamp_ns();
    _timer->log( now - _start_time );
    global_timer().stop<category>( now );
  }

  RecordScopeTimer( const RecordScopeTimer& ) = delete;
  RecordScopeTimer& operator=( const RecordScopeTimer& ) = delete;
};
