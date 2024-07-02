#include "option-parser.hh"
#include "relater.hh"
#include "scheduler.hh"
#include <glog/logging.h>
#include <memory>

using namespace std;

void test( std::shared_ptr<Relater> );

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  OptionParser parser( "fixpoint-test", "Run a fixpoint test" );
  optional<string> sche_opt;
  parser.AddOption( 's', "scheduler", "scheduler", "Scheduler to use [local, hint]", [&]( const char* argument ) {
    sche_opt = argument;
    if ( not( *sche_opt == "hint" or *sche_opt == "local" ) ) {
      throw runtime_error( "Invalid scheduler: " + sche_opt.value() );
    }
  } );

  google::InitGoogleLogging( argv[0] );
  google::SetStderrLogging( google::INFO );
  google::InstallFailureSignalHandler();

  parser.Parse( argc, argv );

  shared_ptr<Scheduler> scheduler = make_shared<HintScheduler>();
  if ( sche_opt.has_value() ) {
    if ( *sche_opt == "local" ) {
      scheduler = make_shared<LocalScheduler>();
    }
  }

  auto rt = std::make_shared<Relater>( std::thread::hardware_concurrency(), std::nullopt, scheduler );

  test( rt );
}
