#include <stdio.h>

#include "handle.hh"
#include "overload.hh"
#include "runtimestorage.hh"

#include <glog/logging.h>

using namespace std;

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

void test( void )
{
  RuntimeStorage storage;

  Handle virgil = storage.create( aeneid );
  Handle caesar = storage.create( de_bello_gallico );

  Handle<Combination> combination
    = storage.construct_tree<ExpressionTree>( "unused"_literal, "elf"_literal, caesar );

  Handle<Identity> hidden = storage.construct_tree<ObjectTree>( "not visible"_literal );

  auto full
    = storage.construct( "visible 1"_literal, "visible 2"_literal, hidden, virgil, combination, 100_literal32 );

  {
    size_t count = 0;
    bool saw_aeneid = false;
    storage.visit( Handle<Fix>( full ), [&]( Handle<Fix> h ) {
      count++;
      std::visit( overload { []( const Handle<Literal> ) { CHECK( false ); },
                             [&]( const auto other ) {
                               CHECK( other.is_local() );
                               CHECK_NE( Handle<Fix>( other ), Handle<Fix>( caesar ) );
                             } },
                  fix_data( h ) );
      if ( h == Handle<Fix>( virgil ) )
        saw_aeneid = true;
    } );
    CHECK_EQ( count, 2 );
    CHECK( saw_aeneid );
  }
  {
    size_t count = 0;
    bool saw_aeneid = false;
    storage.visit( Handle<Fix>( storage.canonicalize( full ) ), [&]( Handle<Fix> h ) {
      count++;
      std::visit( overload { []( const Handle<Literal> ) { CHECK( false ); },
                             [&]( const auto other ) {
                               CHECK( not other.is_local() );
                               CHECK_NE( Handle<Fix>( other ), Handle<Fix>( caesar ) );
                             } },
                  fix_data( h ) );
      if ( h == Handle<Fix>( storage.canonicalize( virgil ) ) )
        saw_aeneid = true;
    } );
    CHECK_EQ( count, 2 );
    CHECK( saw_aeneid );
  }
  {
    size_t count = 0;
    storage.visit_full( Handle<Fix>( full ), [&]( Handle<Fix> ) { count++; } );
    CHECK_EQ( count, 5 );
  }
  {
    size_t count = 0;
    storage.visit_full( Handle<Fix>( storage.canonicalize( full ) ), [&]( Handle<Fix> ) { count++; } );
    CHECK_EQ( count, 5 );
  }

  {
    Handle<Apply> apply( virgil );
    Handle<Fix> target( caesar );
    storage.create( apply, target );
    CHECK( storage.contains( apply ) );
    CHECK( storage.complete( apply ) );
    CHECK_EQ( storage.get( apply ), target );
    auto canonical = storage.canonicalize( apply );
    CHECK_NE( storage.get( canonical ), target );
    CHECK_NE( storage.get( apply ), target );
    CHECK_EQ( storage.get( canonical ), storage.canonicalize( target ) );
    CHECK_EQ( storage.get( apply ), storage.canonicalize( target ) );
  }
}
