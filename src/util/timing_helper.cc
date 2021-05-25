#include "timing_helper.hh"

using namespace std;

void print_timer( ostream& out, const string_view name, const Timer::Record timer )
{
  if ( timer.count == 0 ) {
    return;
  }

  out << "   " << name << ": ";
  out << string( 27 - name.size(), ' ' );
  out << "mean ";
  Timer::pp_ns( out, timer.total_ns / timer.count );

  out << "  [";
  Timer::pp_ns( out, timer.min_ns );
  out << "..";
  Timer::pp_ns( out, timer.max_ns );
  out << "]";

  out << " N=" << timer.count;
  out << "\n";
}
