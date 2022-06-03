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
  Timer::pp_ticks( out, timer.total_ticks / timer.count );

  out << "  [";
  Timer::pp_ticks( out, timer.min_ticks );
  out << "..";
  Timer::pp_ticks( out, timer.max_ticks );
  out << "]";

  out << " N=" << timer.count;
  out << "\n";
}

void print_fixpoint_timers( std::ostream& out )
{
#if TIME_FIXPOINT == 2
  print_timer( out, "_attach_blob", _attach_blob );
  print_timer( out, "_create_blob", _create_blob );
  print_timer( out, "_attach_tree", _attach_tree );
  print_timer( out, "_freeze_blob", _create_tree );
#elif TIME_FIXPOINT == 1
  print_timer( out, "_execution", _execution );
#endif
}
