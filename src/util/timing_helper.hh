#pragma once

#include <iostream>

#include "timer.hh"

#ifndef TIME_FIXPOINT_API
#define TIME_FIXPOINT_API 0
#endif

inline Timer::Record _hash;
inline Timer::Record _fixpoint_apply;
inline Timer::Record _get_tree_entry;
inline Timer::Record _attach_blob;
inline Timer::Record _detach_mem;
inline Timer::Record _designate_output;
inline Timer::Record _freeze_blob;

void print_timer( std::ostream& out, const std::string_view name, const Timer::Record timer );

void print_fixpoint_timers( std::ostream& out );
