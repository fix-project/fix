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
  CHECK_EQ( storage.get( virgil.unwrap<Named>() )->size(), aeneid.size() );
  Handle caesar = storage.create( de_bello_gallico );
  CHECK_EQ( storage.get( caesar.unwrap<Named>() )->size(), de_bello_gallico.size() );

  /* Handle<Application> combination */
  /*   = storage.construct_tree<ExpressionTree>( "unused"_literal, "elf"_literal, caesar ); */

  /* Handle<Identification> hidden = storage.construct_tree<ValueTree>( "not visible"_literal ); */

  /* auto full */
  /*   = storage.construct( "visible 1"_literal, "visible 2"_literal, hidden, virgil, combination, 100_literal32 );
   */

  // TODO: reimplement visiting and add tests here
  Handle<ValueTree> tree = storage.construct_tree<ValueTree>( virgil );
  CHECK( storage.contains( tree ) );

  Handle<Think> apply = Handle<Thunk>( Handle<Application>( Handle<ExpressionTree>( tree ) ) );
  Handle<Object> target( caesar );

  storage.create( target, apply );
  CHECK( storage.contains( apply ) );
  CHECK_EQ( storage.get( apply ), target );
  CHECK_EQ( apply.unwrap<Thunk>().unwrap<Application>().unwrap<ExpressionTree>().size(),
            sizeof( Handle<Fix> ) + virgil.unwrap<Named>().size() );

  auto ref = storage.ref( tree );
  CHECK_EQ( ref.unwrap<ValueTreeRef>().size(), 1 );
  auto unref = storage.contains( ref );
  CHECK( unref.has_value() );
  CHECK_EQ( unref.value().unwrap<ValueTree>(), tree );
}
