#include "base16.hh"
#include "blake3.hh"
#include <blake3.h>
#include <glog/logging.h>

using namespace std;

void test( void )
{
  u8x32 hash;

  // "hello world" test
  string_view test1_s1 = "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24";
  string_view test1_s2 = "hello world";
  blake3::encode( test1_s2, hash );
  CHECK_EQ( base16::encode( hash ), test1_s1 );

  // empty string test
  string_view test2_s1 = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262";
  string_view test2_s2 = "";
  blake3::encode( test2_s2, hash );
  CHECK_EQ( base16::encode( hash ), test2_s1 );

  // extremely long string test
  string_view test3_s1 = "711d65565a7d0c3618758f9fefa1b1a2bb9ba60fc3ed4f468a7c2f7c9878004d";
  string_view test3_s2 = "When forty winters shall beseige thy brow,And dig deep trenches in thy beauty's  ";
  blake3::encode( test3_s2, hash );
  CHECK_EQ( base16::encode( hash ), test3_s1 );
}
