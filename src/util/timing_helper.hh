#pragma once

#include <iostream>

#include "timer.hh"

inline Timer::Record _mmap;

void print_timer ( std::ostream& out, const std::string_view name, const Timer::Record timer ); 
