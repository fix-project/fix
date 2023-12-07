#include "base16.hh"
#include "handle.hh"
#include "runtimestorage.hh"
#include "spans.hh"
#include "tester-utils.hh"

#include <filesystem>
#include <fstream>
#include <string>

using namespace std;

void usage_message( const char* argv0 )
{
  cerr << "Usage: " << argv0 << " commands...\n";
  cerr << "   command (followed by entry) :=   what:\n";
  cerr << "                                    -- print the handle to the object\n";
  cerr << "                                  | content:\n";
  cerr << "                                    -- print content of the object\n";
  cerr << "                                  | deep-content:\n";
  cerr << "                                    -- same as content but prints tree content recursively\n";
  cerr << "                                  | result-<operation>: (with <operation> = apply | eval)\n";
  cerr << "                                    -- print the result of eval/apply(object) if known\n";
  cerr << "                                  | steps-eval:\n";
  cerr << "                                    -- print all dependencies of an eval-relation\n";
  cerr << "                                  | step-eval:\n";
  cerr << "                                    -- print the immediate dependencies of an eval-relation\n";
  cerr << "                                  | explain:\n";
  cerr << "                                    -- print explanations for an object\n";
  cerr << "   entry :=   file:<filename>\n";
  cerr << "            | string:<string>\n";
  cerr << "            | name:<base16-encoded name>\n";
  cerr << "            | short-name:<7 bytes shortened base16-encoded name>\n";
  cerr << "            | uint<n>:<integer> (with <n> = 8 | 16 | 32 | 64)\n";
  cerr << "            | tree:<n> (followed by <n> entries)\n";
  cerr << "            | thunk: (followed by tree:<n>)\n";
  cerr << "            | compile:<filename>\n";
  cerr << "            | ref:<ref>\n";
  cerr << "\U0001F4AC means there are more explanations. If it follows a Handle, use \"explain:\". If it follows a Relation, use \"step-eval:\"\n";
}

int min_args = 2;
int max_args = -1;

void parse_args( span_view<char*> args )
{
  if ( args.size() == 0 )
    return;

  auto& rt = Runtime::get_instance( 0 );

  vector<ReadOnlyFile> open_files;

  string_view str { args[0] };
  args.remove_prefix( 1 );
  Handle handle = parse_args( args, open_files, true );

  if ( str.starts_with( "what:" ) ) {
    cout << pretty_print_handle( handle ) << endl;
    return;
  }

  if ( str.starts_with( "deep-content:" ) ) {
    if ( not rt.storage().contains( handle ) ) {
      cerr << "Object does not exist\n";
      return;
    }
    cout << deep_pretty_print( handle );
    return;
  }

  if ( str.starts_with( "content:" ) ) {
    if ( not rt.storage().contains( handle ) ) {
      cerr << "Object does not exist\n";
      return;
    }
    cout << pretty_print( handle );
    return;
  }

  if ( str.starts_with( "result-" ) ) {
    str.remove_prefix( 7 );

    optional<Relation> relation;
    if ( str.starts_with( "apply:" ) ) {
      relation = rt.get_task_relation( Task::Apply( handle ) );
    } else if ( str.starts_with( "eval:" ) ) {
      relation = rt.get_task_relation( Task::Eval( handle ) );
    } else {
      cerr << "Cannot parse command\n";
      return;
    }
    if ( not relation.has_value() ) {
      cerr << "Relation does not exist\n";
      return;
    }

    cout << relation.value() << endl;
    return;
  }

  if ( str.starts_with( "step-eval:" ) ) {
    auto relation = rt.get_task_relation( Task::Eval( handle ) );

    if ( not relation.has_value() ) {
      cerr << "Relation does not exist\n";
      return;
    }
    rt.shallow_visit( relation.value(),
                      []( Relation relation ) { cout << pretty_print_relation( relation ) << endl; } );
    return;
  }

  if ( str.starts_with( "steps-eval:" ) ) {
    auto relation = rt.get_task_relation( Task::Eval( handle ) );

    if ( not relation.has_value() ) {
      cerr << "Relation does not exist\n";
      return;
    }
    rt.visit( relation.value(), []( Relation relation ) { cout << pretty_print_relation( relation ) << endl; } );
    return;
  }

  if ( str.starts_with( "explain:" ) ) {
    auto [relations, names] = rt.get_explanations( handle );

    cout << "Relations that point to the handle:\n";
    for ( const auto& relation : relations ) {
      cout << pretty_print_relation( relation ) << endl;
    }

    if ( names.empty() )
      return;
    cout << "\n";
    cout << "We also have some extra info:\n";
    for ( const auto& name : names ) {
      if ( name.is_tag() ) {
        auto view = rt.storage().get_tree( name );
        if ( rt.storage().compare_handles( view[0], handle ) ) {
          cout << "In " << pretty_print_handle( name ) << ", ";
          cout << pretty_print_handle( view[1] ) << " tags it by\n";
          cout << deep_pretty_print( view[2] ) << endl;
          continue;
        }
      }
      cout << deep_pretty_print( name ) << endl;
    }
    return;
  }
}

void program_body( span_view<char*> args )
{
  args.remove_prefix( 1 );
  parse_args( args );
}
