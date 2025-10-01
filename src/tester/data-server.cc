#include "option-parser.hh"
#include "runtimes.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  OptionParser parser( "data-server", "Run a data server" );

  uint16_t port;
  size_t latency;
  parser.AddArgument(
    "listening-port", OptionParser::ArgumentCount::One, [&]( const char* argument ) { port = stoi( argument ); } );
  parser.AddOption(
    'r', "response-latency", "response-latency", "Latency of request responess in us", [&]( const char* argument ) {
      latency = stoi( argument );
    } );

  parser.Parse( argc, argv );

  DataServer::latency = latency;
  Address listen_address( "0.0.0.0", port );

  auto server = DataServerRT::init( listen_address );

  server->join();

  return 0;
}
