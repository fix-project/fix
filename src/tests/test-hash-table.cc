#include "handle.hh"
#include "hash_table.hh"
#include "runtimestorage.hh"
#include <glog/logging.h>

using namespace std;

const string aeneid = "Arma virumque canō, Trōiae quī prīmus ab ōrīs\n"
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

const string de_bello_gallico
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
  FixTable<Blob, string, AbslHash> test_table( 10 );

  test_table.insert( Handle<Blob>( Handle<Literal>( "one" ) ), aeneid );
  test_table.insert( Handle<Blob>( Handle<Literal>( "two" ) ), de_bello_gallico );

  CHECK( test_table.contains( Handle<Blob>( Handle<Literal>( "one" ) ) ) );
  CHECK( test_table.contains( Handle<Blob>( Handle<Literal>( "two" ) ) ) );
  CHECK( !test_table.contains( Handle<Blob>( Handle<Literal>( "three" ) ) ) );

  CHECK_EQ( test_table.get( Handle<Blob>( Handle<Literal>( "one" ) ) ).value(), aeneid );
  CHECK_EQ( test_table.get( Handle<Blob>( Handle<Literal>( "two" ) ) ).value(), de_bello_gallico );

  test_table.insert( Handle<Blob>( Handle<Literal>( "one" ) ), de_bello_gallico );
  CHECK_EQ( test_table.get( Handle<Blob>( Handle<Literal>( "one" ) ) ).value(), aeneid );
}
