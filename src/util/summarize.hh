#pragma once

#include <ostream>

class Summarizable
{
public:
  virtual void summary( std::ostream& out ) const = 0;
  virtual void reset_summary() {}
  virtual ~Summarizable() {}
};
