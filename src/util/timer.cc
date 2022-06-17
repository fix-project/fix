#include "timer.hh"
#include "exception.hh"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>

using namespace std;

uint64_t Timer::baseline_ = 0;

void Timer::summary( ostream& out ) const
{
  const uint64_t now = __rdtsc();

  const uint64_t elapsed = now - _beginning_timestamp;

  out << "Global timing summary\n---------------------\n\n";

  out << "Total time: ";
  out << now - _beginning_timestamp;
  out << "\n";

  out << "Baseline time mean: ";
  out << Timer::baseline_;
  out << "\n";

  uint64_t accounted = 0;

  for ( unsigned int i = 0; i < num_categories; i++ ) {
    if ( _records.at( i ).count == 0 )
      continue;
    out << "   " << _category_names.at( i ) << ": ";
    out << string( 32 - strlen( _category_names.at( i ) ), ' ' );
    out << fixed << setw( 6 ) << setprecision( 1 ) << 100 * _records.at( i ).total_ticks / double( elapsed ) << "%";
    accounted += _records.at( i ).total_ticks;

    if ( _records.at( i ).count > 0 ) {
      out << "   [mean=";
      out << fixed << setw( 6 ) << _records.at( i ).total_ticks / _records.at( i ).count - Timer::baseline_;
      out << "] ";
    } else {
      out << "                 ";
    }

    out << "[max= ";
    out << fixed << setw( 6 ) << _records.at( i ).max_ticks;
    out << "]";
    out << " [count=" << fixed << setw( 6 ) << _records.at( i ).count << "]";

    out << "\n";
  }

  const uint64_t unaccounted = elapsed - accounted;
  out << "\n   Unaccounted: " << string( 23, ' ' );
  out << 100 * unaccounted / double( elapsed ) << "%\n";
}

void Timer::average( ostream& out, int count ) const
{
  for ( unsigned int i = 0; i < num_categories; i++ ) {
    if ( _records.at( i ).count == 0 )
      continue;
    out << "   " << _category_names.at( i ) << ": ";
    out << string( 32 - strlen( _category_names.at( i ) ), ' ' );

    if ( _records.at( i ).count > 0 ) {
      out << "   [mean=";
      out << fixed << setw( 6 )
          << ( _records.at( i ).total_ticks - _records.at( i ).count * Timer::baseline_ ) / count;
      out << "] ";
    } else {
      out << "                 ";
    }

    out << "[total= ";
    out << fixed << setw( 6 ) << _records.at( i ).total_ticks;
    out << "]";
    out << " [count=" << fixed << setw( 6 ) << count << "]";

    out << "\n";
  }
}
