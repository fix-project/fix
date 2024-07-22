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
  vector<Handle<AnyDataType>> todos_ {};
  uint32_t parallelism_ {};

  optional<BlobData> get( Handle<Named> name ) override
  {
    todos_.push_back( name );
    return {};
  }
  optional<TreeData> get( Handle<AnyTree> name ) override
  {
    todos_.push_back( handle::upcast( name ) );
    return {};
  };
  optional<Handle<Object>> get( Handle<Relation> name ) override
  {
    todos_.push_back( name );
    return {};
  }

  optional<Handle<AnyTree>> get_handle( Handle<AnyTree> handle ) override { return storage_.get_handle( handle ); }

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
  fake_worker->parallelism_ = 10;

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

  if ( fake_worker->todos_.size() != 1
       or fake_worker->todos_.front() != Handle<AnyDataType>( Handle<Relation>( task ) ) ) {
    fprintf( stderr, "Todo size %zu", fake_worker->todos_.size() );
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
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

const std::string aeneid = "Arma virumque canō, Trōiae quī prīmus ab ōrīs\n"
                           "Ītaliam, fātō profugus, Lāvīniaque vēnit\n"
                           "lītora, multum ille et terrīs iactātus et altō\n"
                           "vī superum saevae memorem Iūnōnis ob īram;\n"
                           "multa quoque et bellō passus, dum conderet urbem\n"
                           "inferretque deōs Latiō, genus unde Latīnum,\n"
                           "Albānīque patrēs, atque altae moenia Rōmae.\n"
                           "\n"
                           "Mūsa, mihī causās memorā, quō nūmine laesō,\n"
                           "quidve dolēns, rēgīna deum tot volvere cāsūs\n"
                           "īnsīgnem pietāte virum, tot adīre labōrēs\n"
                           "impulerit. Tantaene animīs caelestibus īrae? \n";

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( uint32_t( 1 ), Handle<Strict>(
// Handle<Identification>( Large Object 0 ) ) ) ) )
// Machine 0: 1 parallelism
// Machine 1: 1 parallelism + Object 0
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

  if ( fake_worker->todos_.size() != 1
       or fake_worker->todos_.front() != Handle<AnyDataType>( Handle<Relation>( task ) ) ) {
    fprintf( stderr, "Case 2: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( Handle<Strict>( Handle<Identification>( Blob 0
// ), Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) ) )
// Machine 0: 1 parallelism + Blob 0
// Machine 1: 1 parallelism + Blob 1 (Blob 0 > Blob 1)
// Expected outcome: get( Blob 1 ) is assigned to machine 1
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
       or ( fake_worker->todos_.front()
              != Handle<AnyDataType>( Handle<Relation>(
                Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle.unwrap<Named>() ) ) ) ) )
            and fake_worker->todos_.front() != Handle<AnyDataType>( handle.unwrap<Named>() ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
    fprintf( stderr, "Case 3: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( Handle<ExpressionTree>( Handle<Strict>( Handle<Identification>( Blob 0
// ), Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) ) )
// Machine 0: 10 parallelism + Blob 0
// Machine 1: 10 parallelism + Blob 1 (Blob 0 > Blob 1)
// Expected outcome: get( Blob 1 ) is assigned to machine 1
void case_four( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 10, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 10;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>( handle::upcast(
    tree( *rt,
          Handle<Strict>(
            handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value() ),
          Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) );
  rt->get( task );

  if ( fake_worker->todos_.size() != 1
       or ( fake_worker->todos_.front()
              != Handle<AnyDataType>( Handle<Relation>(
                Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle.unwrap<Named>() ) ) ) ) )
            and fake_worker->todos_.front() != Handle<AnyDataType>( handle.unwrap<Named>() ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
    fprintf( stderr, "Case 4: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( {1, 1, 1}, Handle<ExpressionTree>
//   ( Handle<Strict>( Handle<Identification>( Blob 0 ) ),
//     Handle<Strict>( Handle<Application>( { 1, 3000, 1 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
//   ) ) )
// Machine 0: 10 parallelism + Blob 0 (very big)
// Machine 1: 10 parallelism + Blob 1 (1269)
// Expected outcome: get( Blob 1 ) or (Blob 1) is assigned to machine 1 but Handle<Application>( { 1, 3000, 1 } ) is
// not
void case_five( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 10, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 10;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>( handle::upcast(
    tree( *rt,
          limits( *rt, 1024, 1, 1 ),
          Handle<Strict>(
            handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value() ),
          Handle<Strict>( Handle<Application>( handle::upcast( tree(
            *rt, limits( *rt, 1, 3000, 1 ), Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ) ) ) ) );

  rt->get( task );

  if ( fake_worker->todos_.size() != 1
       or ( fake_worker->todos_.front()
              != Handle<AnyDataType>( Handle<Relation>(
                Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle.unwrap<Named>() ) ) ) ) )
            and fake_worker->todos_.front() != Handle<AnyDataType>( handle.unwrap<Named>() ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
    fprintf( stderr, "Case 5: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( { 1024, 1, 1}, Handle<ExpressionTree>
//   ( Handle<Strict>( Handle<Application>( { 1, 4000, 10 }, Handle<Strict>( Handle<Identification>( Blob 0 ) ),
//   Handle<Strict>( Handle<Identification>( Blob 1 )> ) ) ),
//     Handle<Strict>( Handle<Application>( { 1, 3000, 10 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
//   ) ) )
// Machine 0: 5 parallelism + Blob 0 (very big)
// Machine 1: 5 parallelism + Blob 1 (1269)
// Expected outcome: get( Blob 1 ) or ( Blob 1 ) is assigned to remote
void case_six( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 5, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 5;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>( handle::upcast( tree(
    *rt,
    limits( *rt, 1024, 1, 1 ),
    Handle<Strict>( Handle<Application>( handle::upcast( tree(
      *rt,
      limits( *rt, 1, 4000, 10 ),
      Handle<Strict>(
        handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value() ),
      Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ),
    Handle<Strict>( Handle<Application>( handle::upcast(
      tree( *rt, limits( *rt, 1, 3000, 10 ), Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ) ) ) ) );

  rt->get( task );

  if ( fake_worker->todos_.size() != 1
       or ( fake_worker->todos_.front()
              != Handle<AnyDataType>( Handle<Relation>(
                Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle.unwrap<Named>() ) ) ) ) )
            and fake_worker->todos_.front() != Handle<AnyDataType>( handle.unwrap<Named>() ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
    fprintf( stderr, "Case 6: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<Application>( { 1024, 1, 1}, Handle<ExpressionTree>
//   ( Handle<Strict>( Handle<Application>( { 1, 4000, 10 }, Handle<Strict>( Handle<Identification>( Blob 0 ) ),
//   Handle<Strict>( Handle<Identification>( Blob 1 )> ) ) ),
//     Handle<Strict>( Handle<Application>( { 1, 1, 10 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
//   ) ) )
// Machine 0: 5 parallelism + Blob 0 (very big)
// Machine 1: 5 parallelism + Blob 1 (1269)
// Expected outcome: Handle<Strict>( Handle<Application>( { 1, 1, 10 }, Handle<Strict>( Handle<Identification>(
// Blob 1 ) ) ) )
//                   and get( Blob 1 ) is assigned to remote
void case_seven( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 5, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 5;

  auto handle = fake_worker->storage_.create( de_bello_gallico );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>( Handle<Application>( handle::upcast( tree(
    *rt,
    limits( *rt, 1024, 1, 1 ),
    Handle<Strict>( Handle<Application>( handle::upcast( tree(
      *rt,
      limits( *rt, 1, 4000, 10 ),
      Handle<Strict>(
        handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value() ),
      Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ),
    Handle<Strict>( Handle<Application>( handle::upcast(
      tree( *rt, limits( *rt, 1, 1, 10 ), Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ) ) ) ) );

  rt->get( task );

  if ( fake_worker->todos_.size() == 2 ) {
    // get( Blob 1 )
    auto job0 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle.unwrap<Named>() ) ) ) ) );
    auto job0_1 = Handle<AnyDataType>( handle.unwrap<Named>() );
    // Handle<Application( { 1, 1, 10 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
    auto job1 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Object>( Handle<Thunk>( Handle<Application>( handle::upcast(
        tree( *rt, limits( *rt, 1, 1, 10 ), Handle<Strict>( Handle<Identification>( handle ) ) ) ) ) ) ) ) ) );
    if ( ( ( fake_worker->todos_[0] == job0 or fake_worker->todos_[0] == job0_1 )
           and fake_worker->todos_[1] == job1 )
         or ( ( fake_worker->todos_[1] == job0 or fake_worker->todos_[1] == job0_1 )
              and fake_worker->todos_[0] == job1 ) ) {
      return;
    }
  }

  cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
  for ( const auto& todo : fake_worker->todos_ ) {
    cout << "Todo " << todo << endl;
  }
  fprintf( stderr, "Case 7: Wrong post condition" );
  exit( 1 );
}

// Work: Handle<Eval>( Handle<ObjectTree>
//   ( Handle<Identification>( Blob 0 ) ),
//     Handle<ObjectTree> (
//       Handle<Identification>( Blob 1 ) ),
//       Handle<Identification>( Blob 2 ) ),
//     )
//   )
//)
// Machine 0: 5 parallelism + Blob 0 (very big)
// Machine 1: 5 parallelism + Blob 1 (1269) + Blob 2 (small)
//
// Expected: Handle<Eval>( Handle<ObjectTree> ( Handle<Identification>( Blob 1 ), Handle<Identification>( Blob 2 ) )
// is assigned to remote
void case_eight( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 5, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 5;

  auto handle1 = fake_worker->storage_.create( de_bello_gallico );
  auto handle2 = fake_worker->storage_.create( aeneid );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>(
    tree( *rt,
          handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value(),
          tree( *rt,
                handle::extract<Identification>( make_identification( handle1 ) ).value(),
                handle::extract<Identification>( make_identification( handle2 ) ).value() )
            .unwrap<ObjectTree>() )
      .unwrap<ObjectTree>() );

  rt->get( task );

  auto remote_task = Handle<Eval>( tree( *rt,
                                         handle::extract<Identification>( make_identification( handle1 ) ).value(),
                                         handle::extract<Identification>( make_identification( handle2 ) ).value() )
                                     .unwrap<ObjectTree>() );

  if ( fake_worker->todos_.size() != 1
       or fake_worker->todos_.front() != Handle<AnyDataType>( Handle<Relation>( remote_task ) ) ) {
    cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
    for ( const auto& todo : fake_worker->todos_ ) {
      cout << "Todo " << todo << endl;
    }
    fprintf( stderr, "Case 8: Wrong post condition" );
    exit( 1 );
  }
}

// Work: Handle<Eval>( Handle<ObjectTree>
//   ( Handle<Identification>( Blob 0 ) ),
//     Handle<ObjectTree> (
//       Handle<Application>( { 1, 3000, 1 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ),
//       Handle<Application>( { 1, 3000, 1 }, Handle<Strict>( Handle<Identification>( Blob 2 ) ) ),
//     )
//   )
//)
// Machine 0: 5 parallelism + Blob 0 (very big)
// Machine 1: 5 parallelism + Blob 1 (1269) + Blob 2 (small)
//
// Expected: get( Blob1 ) and get( Blob2 ) executed remotely
void case_nine( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 5, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 5;

  auto handle1 = fake_worker->storage_.create( de_bello_gallico );
  auto handle2 = fake_worker->storage_.create( aeneid );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>(
    tree(
      *rt,
      handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value(),
      tree( *rt,
            Handle<Application>( handle::upcast( tree(
              *rt,
              limits( *rt, 1, 3000, 1 ),
              Handle<Strict>( handle::extract<Identification>( make_identification( handle1 ) ).value() ) ) ) ),
            Handle<Application>( handle::upcast( tree(
              *rt,
              limits( *rt, 1, 3000, 1 ),
              Handle<Strict>( handle::extract<Identification>( make_identification( handle2 ) ).value() ) ) ) ) )
        .unwrap<ObjectTree>() )
      .unwrap<ObjectTree>() );

  rt->get( task );

  if ( fake_worker->todos_.size() == 2 ) {
    // get( Blob 1 )
    auto job0 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle1.unwrap<Named>() ) ) ) ) );
    // get( Blob 2 )
    auto job1 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle2.unwrap<Named>() ) ) ) ) );
    // Handle<Application( { 1, 1, 10 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
    if ( ( fake_worker->todos_[0] == job0 and fake_worker->todos_[1] == job1 )
         or ( fake_worker->todos_[1] == job0 and fake_worker->todos_[0] == job1 ) ) {
      return;
    }
  }

  cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
  for ( const auto& todo : fake_worker->todos_ ) {
    cout << "Todo " << todo << endl;
  }
  fprintf( stderr, "Case 9: Wrong post condition" );
  exit( 1 );
}

// Work: Handle<Eval>( Handle<ObjectTree>
//   ( Handle<Identification>( Blob 0 ) ),
//     Handle<ObjectTree> (
//       Handle<Application>( { 1, 1, 1 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ),
//       Handle<Application>( { 1, 3000, 1 }, Handle<Strict>( Handle<Identification>( Blob 2 ) ) ),
//     )
//   )
//)
// Machine 0: 5 parallelism + Blob 0 (very big)
// Machine 1: 5 parallelism + Blob 1 (1269) + Blob 2 (small)
//
// Expected: Handle<Eval>( Handle<Application>( { 1, 1, 1 } ) ) and get( Blob2 ) executed remotely
void case_ten( void )
{
  shared_ptr<Relater> rt = make_shared<Relater>( 5, make_shared<PointerRunner>(), make_shared<HintScheduler>() );
  shared_ptr<FakeRuntime> fake_worker = make_shared<FakeRuntime>();
  fake_worker->parallelism_ = 5;

  auto handle1 = fake_worker->storage_.create( de_bello_gallico );
  auto handle2 = fake_worker->storage_.create( aeneid );
  rt->add_worker( fake_worker );

  auto task = Handle<Eval>(
    tree(
      *rt,
      handle::extract<Identification>( make_identification( rt->labeled( "c-to-elf-fix-wasm" ) ) ).value(),
      tree( *rt,
            Handle<Application>( handle::upcast( tree(
              *rt,
              limits( *rt, 1, 1, 1 ),
              Handle<Strict>( handle::extract<Identification>( make_identification( handle1 ) ).value() ) ) ) ),
            Handle<Application>( handle::upcast( tree(
              *rt,
              limits( *rt, 1, 3000, 1 ),
              Handle<Strict>( handle::extract<Identification>( make_identification( handle2 ) ).value() ) ) ) ) )
        .unwrap<ObjectTree>() )
      .unwrap<ObjectTree>() );

  rt->get( task );

  if ( fake_worker->todos_.size() == 2 ) {
    // get( Blob 1 )
    auto job0 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Thunk>( Handle<Application>( handle::upcast( tree(
        *rt,
        limits( *rt, 1, 1, 1 ),
        Handle<Strict>( handle::extract<Identification>( make_identification( handle1 ) ).value() ) ) ) ) ) ) ) );
    // get( Blob 2 )
    auto job1 = Handle<AnyDataType>(
      Handle<Relation>( Handle<Eval>( Handle<Thunk>( Handle<Identification>( handle2.unwrap<Named>() ) ) ) ) );
    // Handle<Application( { 1, 1, 10 }, Handle<Strict>( Handle<Identification>( Blob 1 ) ) ) )
    if ( ( fake_worker->todos_[0] == job0 and fake_worker->todos_[1] == job1 )
         or ( fake_worker->todos_[1] == job0 and fake_worker->todos_[0] == job1 ) ) {
      return;
    }
  }

  cout << "fake_worker->todos_.size " << fake_worker->todos_.size() << endl;
  for ( const auto& todo : fake_worker->todos_ ) {
    cout << "Todo " << todo << endl;
  }
  fprintf( stderr, "Case 9: Wrong post condition" );
  exit( 1 );
}
void test( void )
{
  case_one();
  case_two();
  case_three();
  case_four();
  case_five();
  case_six();
  case_seven();
  case_eight();
  case_nine();
  case_ten();
}
