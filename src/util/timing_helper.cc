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

void print_fixpoint_timers( std::ostream& out )
{
#if TIME_FIXPOINT_API
  print_timer( out, "_attach_blob", _attach_blob );
  print_timer( out, "_get_tree_entry", _get_tree_entry );
  print_timer( out, "_detach_mem", _detach_mem );
  print_timer( out, "_freeze_blob", _freeze_blob );
  print_timer( out, "_designate_output", _designate_output );
#else
  print_timer( out, "_fixpoint_apply", _fixpoint_apply );
#endif
}
