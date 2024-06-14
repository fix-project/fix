#include "handle.hh"
#include "scheduler.hh"
#include "test.hh"

using namespace std;

class RelaterTest
{
public:
  static void relate( SketchGraphScheduler& sch, Handle<Relation> r ) { sch.relate( r ); }
};

void case0()
{
  auto rt = make_shared<Relater>();
  OnePassScheduler sch;
  sch.set_relater( *rt.get() );

  auto thunk0 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "zero" ) ) ) );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );
  auto thunk2 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "two" ) ) ) );

  rt->put( Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "zero" ) ).unwrap<ValueTree>() ) ),
           thunk1 );
  auto thunk_tree = tree( *rt, thunk0, thunk2 );

  RelaterTest::relate( sch, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works_.size() != 2 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );
  auto apply2 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "two" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works_[0] == apply1 && works_[1] == apply2 ) && !( works_[0] == apply2 && works_[1] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }
}

void case1()
{
  auto rt = make_shared<Relater>();
  OnePassScheduler sch;
  sch.set_relater( *rt.get() );

  auto thunk0 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "zero" ) ) ) );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );

  rt->put( Handle<Eval>( thunk0 ), Handle<Literal>( "zero" ) );
  auto thunk_tree = tree( *rt, thunk0, thunk1 );

  RelaterTest::relate( sch, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works_.size() != 1 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works_[0] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }
}

void case2()
{
  auto rt = make_shared<Relater>();
  OnePassScheduler sch;
  sch.set_relater( *rt.get() );

  auto thunk0 = Handle<Identification>( tree( *rt, Handle<Literal>( "zero" ) ).unwrap<ValueTree>() );
  auto thunk1 = Handle<Application>( handle::upcast( tree( *rt, Handle<Literal>( "one" ) ) ) );

  rt->put( Handle<Eval>( thunk0 ), Handle<Literal>( "zero" ) );
  auto thunk_tree = tree( *rt, thunk0, thunk1 );

  RelaterTest::relate( sch, Handle<Eval>( thunk_tree.unwrap<ObjectTree>() ) );

  if ( works_.size() != 1 ) {
    fprintf( stderr, "Wrong number of leaf jobs" );
    exit( 1 );
  }

  auto apply1 = Handle<AnyDataType>( Handle<Relation>(
    Handle<Apply>( Handle<ObjectTree>( tree( *rt, Handle<Literal>( "one" ) ).unwrap<ValueTree>() ) ) ) );

  if ( !( works_[0] == apply1 ) ) {
    fprintf( stderr, "Wrong leaf jobs" );
    exit( 1 );
  }
}

void test( void )
{
  case0();
  case1();
  case2();
}
