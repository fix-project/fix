#include "handle.hh"
#include "handle_post.hh"
#include "interface.hh"
#include "relater.hh"
#include "runtimestorage.hh"
#include "scheduler.hh"
#include "test.hh"

#include <cstdio>
#include <memory>

using namespace std;

class FakeRuntime : public IRuntime
{
public:
  RuntimeStorage storage_ {};
  vector<Handle<Relation>> todos_ {};
  uint32_t parallelism_ {};

  optional<BlobData> get( Handle<Named> name ) override { return storage_.get( name ); };
  optional<TreeData> get( Handle<AnyTree> name ) override { return storage_.get( name ); };
  optional<Handle<Object>> get( Handle<Relation> name ) override
  {
    todos_.push_back( name );
    return {};
  }

  void put( Handle<Named> name, BlobData data ) override { storage_.create( data, name ); }
  void put( Handle<AnyTree> name, TreeData data ) override { storage_.create( data, name ); }
  void put( Handle<Relation> name, Handle<Object> data ) override { storage_.create( data, name ); }

  bool contains( Handle<Named> handle ) override { return storage_.contains( handle ); }
  bool contains( Handle<AnyTree> handle ) override { return storage_.contains( handle ); }
  bool contains( Handle<Relation> handle ) override { return storage_.contains( handle ); }

  virtual std::optional<Info> get_info() override { return Info { .parallelism = parallelism_, .link_speed = 10 }; }
};

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( uint32_t( 1 ), Handle<Strict>(
// Handle<Identification>( Large Object 0 ) ), Handle<Strict>( Handle<Identification>( Large Object 1 ) ) ) ))
// Machine 0: 1 parallelism + Object 0 and Object 1
// Machine 1: 10 parallelism + Object 0 and Object 1
// Expected outcome: Whole work is assgined to machine 1 for higher parallelism
void case_one( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 1, make_shared<PointerRunner>(), make_shared<OnePassScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 1;

  Handle<ValueTree> system_dep_tree = rt->labeled( "system-dep-tree" )
                                        .try_into<Expression>()
                                        .and_then( []( auto h ) { return h.template try_into<Object>(); } )
                                        .and_then( []( auto h ) { return h.template try_into<Value>(); } )
                                        .and_then( []( auto h ) { return h.template try_into<ValueTree>(); } )
                                        .value();
  Handle<ValueTree> clang_dep_tree = rt->labeled( "clang-dep-tree" )
                                       .try_into<Expression>()
                                       .and_then( []( auto h ) { return h.template try_into<Object>(); } )
                                       .and_then( []( auto h ) { return h.template try_into<Value>(); } )
                                       .and_then( []( auto h ) { return h.template try_into<ValueTree>(); } )
                                       .value();

  fake_worker->put( system_dep_tree, make_shared<OwnedTree>( OwnedMutTree::allocate( 1 ) ) );
  fake_worker->put( clang_dep_tree, make_shared<OwnedTree>( OwnedMutTree::allocate( 1 ) ) );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>(
    Handle<Application>( handle::upcast( tree( *rt,
                                               1_literal32,
                                               Handle<Strict>( Handle<Identification>( system_dep_tree ) ),
                                               Handle<Strict>( Handle<Identification>( clang_dep_tree ) ) ) ) ) );

  rt->get( task );

  if ( fake_worker->todos_.size() != 1 or fake_worker->todos_.front() != Handle<Relation>( task ) ) {
    fprintf( stderr, "Case 1: Wrong post condition" );
    exit( 1 );
  }
}

const std::string de_bello_gallico
  = "Gallia est omnis divisa in partes tres, quarum unam incolunt Belgae, aliam Aquitani, tertiam qui ipsorum "
    "lingua Celtae, nostra Galli appellantur. Hi omnes lingua, institutis, legibus inter se differunt. Gallos ab "
    "Aquitanis Garumna flumen, a Belgis Matrona et Sequana dividit. Horum omnium fortissimi sunt Belgae, propterea "
    "quod a cultu atque humanitate provinciae longissime absunt, minimeque ad eos mercatores saepe commeant atque "
    "ea quae ad effeminandos animos pertinent important, proximique sunt Germanis, qui trans Rhenum incolunt, "
    "quibuscum continenter bellum gerunt. Qua de causa Helvetii quoque reliquos Gallos virtute praecedunt, quod "
    "fere cotidianis proeliis cum Germanis contendunt, cum aut suis finibus eos prohibent aut ipsi in eorum "
    "finibus bellum gerunt. Eorum una pars, quam Gallos obtinere dictum est, initium capit a flumine Rhodano, "
    "continetur Garumna flumine, Oceano, finibus Belgarum, attingit etiam ab Sequanis et Helvetiis flumen Rhenum, "
    "vergit ad septentriones. Belgae ab extremis Galliae finibus oriuntur, pertinent ad inferiorem partem fluminis "
    "Rheni, spectant in septentrionem et orientem solem. Aquitania a Garumna flumine ad Pyrenaeos montes et eam "
    "partem Oceani quae est ad Hispaniam pertinet; spectat inter occasum solis et septentriones.";

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( uint32_t( 1 ), Handle<Strict>(
// Handle<Identification>( Large Object 0 ) ) ) ) ) Machine 0: 1 parallelism Machine 1: 1 parallelism + Object 0
// Expected outcome: Whole work is assgined to machine 1
void case_two( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 1, make_shared<PointerRunner>(), make_shared<OnePassScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 1;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>(
    handle::upcast( tree( *rt, 1_literal32, Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) );

  rt->get( task );

  if ( fake_worker->todos_.size() != 1 or fake_worker->todos_.front() != Handle<Relation>( task ) ) {
    fprintf( stderr, "Case 2: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( Handle<Strict>( Handle<Identification>( Blob 0
// ), Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) ) ) Machine 0: 1 parallelism + Blob 0 Machine 1: 1
// parallelism + Blob 1 Blob 0 > Blob 1 Expected outcome: Handle<Eval>( Handle<Identification>( large object 1 ) )
// is assigned to machine 1
void case_three( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 1, make_shared<PointerRunner>(), make_shared<OnePassScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 1;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>( handle::upcast(
    tree( *rt,
          Handle<Strict>(
            handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value() ),
          Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) );
  rt->get( task );

  if ( fake_worker->todos_.size() != 1
       or fake_worker->todos_.front() != Handle<Relation>( Handle<Eval>( Handle<Identification>( handle ) ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    fprintf( stderr, "Case 3: Wrong post condition" );
    exit( 1 );
  }
}

void test( void )
{
  case_one();
  case_two();
  case_three();
}
