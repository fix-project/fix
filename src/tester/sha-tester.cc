#include <charconv>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

#include "base64.hh"
#include "tester-utils.hh"

#include "sha256.hh"
#include <glog/logging.h>
using namespace std;

int min_args = 1;
int max_args = 1;
void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " sha256.cc [label]\n";
}

void program_body( span_view<char*> )
{
  // TBD sha256 tests
  // Testing sha256.s "hello fixpoint!"
  hello();
  cout << "Test for sha256 " << endl;
}
