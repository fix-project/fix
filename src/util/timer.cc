#include "timer.hh"
#include "exception.hh"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <ostream>

using namespace std;

void Timer::summary( ostream& out ) const
{
  const uint64_t now = timestamp_ns();

  const uint64_t elapsed = now - _beginning_timestamp;

  out << "Global timing summary\n---------------------\n\n";

  out << "Total time: ";
  pp_ns( out, now - _beginning_timestamp );
  out << "\n";

  uint64_t accounted = 0;

  for ( unsigned int i = 0; i < num_categories; i++ ) {
    out << "   " << _category_names.at( i ) << ": ";
    out << string( 32 - strlen( _category_names.at( i ) ), ' ' );
    out << fixed << setw( 5 ) << setprecision( 1 ) << 100 * _records.at( i ).total_ns / double( elapsed ) << "%";
    accounted += _records.at( i ).total_ns;

    if ( _records.at( i ).count > 0 ) {
      out << "   [mean=";
      pp_ns( out, _records.at( i ).total_ns / _records.at( i ).count );
      out << "] ";
    } else {
      out << "                   ";
    }

    out << "[max= ";
    pp_ns( out, _records.at( i ).max_ns );
    out << "]";
    out << " [count=" << _records.at( i ).count << "]";

    out << "\n";
  }

  const uint64_t unaccounted = elapsed - accounted;
  out << "\n   Unaccounted: " << string( 23, ' ' );
  out << 100 * unaccounted / double( elapsed ) << "%\n";
}
