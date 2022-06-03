#pragma once

#include <iostream>

#include "timer.hh"

#ifndef TIME_FIXPOINT
#define TIME_FIXPOINT 0
#endif

inline Timer::Record _hash;
inline Timer::Record _execution;
inline Timer::Record _attach_blob;
inline Timer::Record _create_blob;
inline Timer::Record _attach_tree;
inline Timer::Record _create_tree;

void print_timer( std::ostream& out, const std::string_view name, const Timer::Record timer );

void print_fixpoint_timers( std::ostream& out );
