#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "interface.hh"
#include "network.hh"
#include "operation.hh"
#include "tester-utils.hh"

using namespace std;
using path = filesystem::path;

class DummyRuntime : public IRuntime
{
  vector<reference_wrapper<ITaskRunner>> runners_ {};

public:
  ITaskRunner& get_runner() { return runners_.back(); }

  optional<Handle> start( Task&& task ) override
  {
    cout << "DummyRuntime: start ";
    switch ( task.operation() ) {
      case Operation::Apply:
        cout << "APPLY";
        break;

      case Operation::Eval:
        cout << "EVAL";
        break;

      case Operation::Fill:
        cout << "FILL";
        break;
    }
    cout << " " << task.handle().literal_blob() << endl;
    return {};
  }

  void add_task_runner( ITaskRunner& runner ) override
  {
    cout << "DummyRuntime: new task runner." << endl;
    runners_.push_back( runner );
  }

  void print_info()
  {
    cout << "Print runner info" << endl;
    for ( size_t i = 0; i < runners_.size(); i++ ) {
      cout << "Runner " << i << ": ";
      auto info = runners_.at( i ).get().get_info();
      if ( info.has_value() ) {
        cout << info.value().parallelism << endl;
      } else {
        cout << "info not ready." << endl;
      }
    }
  }

  void finish( Task&& task, Handle result ) override
  {
    cout << "DummyRuntime : finish TASK: " << task.handle().literal_blob();
    cout << " with RESULT: " << result.literal_blob() << endl;
  }

  optional<Info> get_info() override { return {}; }
  void add_result_cache( [[maybe_unused]] IResultCache& cache ) override {}
};

void server( DummyRuntime& runtime )
{
  NetworkWorker worker { runtime };
  worker.start_server( { "0.0.0.0", 4001 } );
  sleep( 5 );
  runtime.print_info();

  Handle handle( "testing evaling" );
  Handle result( "testing evaling result" );
  Task t( handle, Operation::Eval );
  worker.finish( std::move( t ), result );

  while ( true ) {
    sleep( 1 );
  }
}

void client( DummyRuntime& runtime )
{
  NetworkWorker worker { runtime };
  sleep( 1 );
  worker.connect( { "0.0.0.0", 4001 } );
  sleep( 1 );

  Handle handle( "testing evaling" );
  Task t( handle, Operation::Eval );
  runtime.get_runner().start( move( t ) );

  sleep( 1 );

  Tree testing_tree { 3 };

  testing_tree.at( 0 ) = Handle( "Testing entry 0" );
  testing_tree.at( 1 ) = Handle( "Testing entry 1" );
  testing_tree.at( 2 ) = Handle( "Testing entry 2" );

  worker.send_tree( testing_tree );

  while ( true ) {
    sleep( 1 );
  }
}

int main()
{
  DummyRuntime runtime;
  auto pid = fork();
  if ( pid ) {
    client( runtime );
  } else {
    server( runtime );
  }
}
