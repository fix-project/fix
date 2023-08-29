#include "sha256.hh"
#include "picosha2.h"
#include "timer.hh"

using namespace std;

string sha256::encode( string_view input )
{
  GlobalScopeTimer<Timer::Category::Hash> record_timer;
  string digest( picosha2::k_digest_size, 0 );
  picosha2::hash256( input, digest );
  return digest;
}
