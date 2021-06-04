#pragma once

#include <iostream>

#include "timer.hh"

inline Timer::Record _invocation_init;
inline Timer::Record _memory_init_cheap;
inline Timer::Record _memory_init_expensive;
inline Timer::Record _path_open;
inline Timer::Record _fd_write;
inline Timer::Record _post_execution;
inline Timer::Record _pre_execution;
inline Timer::Record _hash;

void print_timer( std::ostream& out, const std::string_view name, const Timer::Record timer );
