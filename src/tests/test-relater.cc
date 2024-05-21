#include "handle.hh"
#include "relater.hh"
#include "test.hh"

using namespace std;

class RelaterTest
{
public:
  static vector<Handle<AnyDataType>> relate( shared_ptr<Relater> rt, Handle<Relation> r )
  {
    return rt->relate( r );
  }
};

void case0()
{
  auto rt = make_shared<Relater>();

  auto thunk0 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "zero" ) ) ) );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );
  auto thunk2 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "two" ) ) ) );

  rt->put( Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "zero" ) ).unwrap<ValueTree>() ) ),
           thunk1 );
  auto thunk_tree = tree( *rt, thunk0, thunk2 );

  auto works = RelaterTest::relate( rt, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works.size() != 2 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );
  auto apply2 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "two" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works[0] == apply1 && works[1] == apply2 ) && !( works[0] == apply2 && works[1] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }

  if ( must_be_local_.size() != 2 ) {
    fprintf( stderr, "Wrong number of jobs that must happen locally" );
    exit( 1 );
  }

  if ( !must_be_local_.contains( Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) )
       || !must_be_local_.contains( Handle<Eval>( thunk0 ) ) ) {
    fprintf( stderr, "Wrong jobs that must happen locally" );
    exit( 1 );
  }
}

void case1()
{
  auto rt = make_shared<Relater>();

  auto thunk0 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "zero" ) ) ) );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );

  rt->put( Handle<Eval>( thunk0 ), Handle<Literal>( "zero" ) );
  auto thunk_tree = tree( *rt, thunk0, thunk1 );

  auto works = RelaterTest::relate( rt, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works.size() != 1 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works[0] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }

  if ( must_be_local_.size() != 1 ) {
    fprintf( stderr, "Wrong number of jobs that must happen locally" );
    exit( 1 );
  }

  if ( !must_be_local_.contains( Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) ) ) {
    fprintf( stderr, "Wrong jobs that must happen locally" );
    exit( 1 );
  }
}

void case2()
{
  auto rt = make_shared<Relater>();

  auto thunk0 = Handle<Identification>( tree( *rt, Handle<Literal>( "zero" ) ).unwrap<ValueTree>() );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );

  rt->put( Handle<Eval>( thunk0 ), Handle<Literal>( "zero" ) );
  auto thunk_tree = tree( *rt, thunk0, thunk1 );

  auto works = RelaterTest::relate( rt, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works.size() != 1 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works[0] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }

  if ( must_be_local_.size() != 0 ) {
    fprintf( stderr, "Wrong number of jobs that must happen locally" );
    exit( 1 );
  }
}

void test( void )
{
  case0();
  case1();
  case2();
}
