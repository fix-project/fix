#include <charconv>

#include "tester-utils.hh"

#include "base64.hh"

using namespace std;

static bool deserialized = false;

template<std::integral T>
T to_int( const std::string_view str );
template<std::integral T>
T from_int( const std::string_view str );

Handle parse_args( span_view<char*>& args, vector<ReadOnlyFile>& open_files )
{
  auto& runtime = RuntimeStorage::get_instance();

  if ( args.empty() ) {
    throw runtime_error( "not enough args" );
  }

  const string_view str { args[0] };

  if ( str.starts_with( "file:" ) ) {
    open_files.emplace_back( string( str.substr( 5 ) ) );
    args.remove_prefix( 1 );
    return runtime.add_blob( static_cast<string_view>( open_files.back() ) );
  }
  if ( str.starts_with( "compile:" ) ) {
    open_files.emplace_back( string( str.substr( 8 ) ) );
    args.remove_prefix( 1 );
    if ( !deserialized ) {
      runtime.deserialize();
      deserialized = true;
    }
    Tree compile_tree { 3 };
    compile_tree.at( 0 ) = Handle( "unused" );
    compile_tree.at( 1 ) = Handle( base64::decode( COMPILE_ENCODE ) );
    Handle blob = runtime.add_blob( static_cast<string_view>( open_files.back() ) );
    compile_tree.at( 2 ) = blob;
    return runtime.add_thunk( Thunk( runtime.add_tree( std::move( compile_tree ) ) ) );
    // TODO: finish
  }

  if ( str.starts_with( "name:" ) ) {
    if ( !deserialized ) {
      runtime.deserialize();
      deserialized = true;
    }
    args.remove_prefix( 1 );
    return base64::decode( str.substr( 5 ) );
  }

  if ( str.starts_with( "string:" ) ) {
    args.remove_prefix( 1 );
    return runtime.add_blob( str.substr( 7 ) );
  }

  if ( str.starts_with( "uint8:" ) ) {
    args.remove_prefix( 1 );
    return runtime.add_blob( make_blob( to_int<uint8_t>( str.substr( 6 ) ) ) );
  }

  if ( str.starts_with( "uint16:" ) ) {
    args.remove_prefix( 1 );
    return runtime.add_blob( make_blob( to_int<uint16_t>( str.substr( 7 ) ) ) );
  }

  if ( str.starts_with( "uint32:" ) ) {
    args.remove_prefix( 1 );
    return runtime.add_blob( make_blob( to_int<uint32_t>( str.substr( 7 ) ) ) );
  }

  if ( str.starts_with( "uint64:" ) ) {
    args.remove_prefix( 1 );
    return runtime.add_blob( make_blob( to_int<uint64_t>( str.substr( 7 ) ) ) );
  }

  if ( str.starts_with( "tree:" ) ) {
    const uint32_t tree_size = to_int<uint32_t>( str.substr( 5 ) );
    args.remove_prefix( 1 );
    if ( args.size() < tree_size ) {
      throw runtime_error( "not enough args to make Tree of length " + to_string( tree_size ) );
    }

    Tree the_tree { tree_size };
    for ( uint32_t i = 0; i < tree_size; ++i ) {
      the_tree.at( i ) = parse_args( args, open_files );
    }
    return runtime.add_tree( std::move( the_tree ) );
  }

  if ( str.starts_with( "thunk:" ) ) {
    args.remove_prefix( 1 );
    const string_view str1 { args[0] };
    if ( !str1.starts_with( "tree:" ) ) {
      throw runtime_error( "thunk not refering a tree" );
    }

    Handle tree_name = parse_args( args, open_files );
    Thunk the_thunk( tree_name );
    return runtime.add_thunk( the_thunk );
  }

  throw runtime_error( "unknown object syntax: \"" + string( str ) + "\"" );
}

template<integral T>
T to_int( const string_view str )
{
  T ret;

  const auto [ptr, ec] = from_chars( str.data(), str.data() + str.size(), ret );

  if ( ptr != str.data() + str.size() or ec != errc {} ) {
    throw system_error { make_error_code( ec ),
                         "to_int<uint" + to_string( 8 * sizeof( T ) ) + "_t>( \"" + string( str ) + "\" )" };
  }

  return ret;
}

template<integral T>
T from_int( const string_view str )
{
  T ret;
  if ( sizeof( T ) != str.size() ) {
    throw runtime_error( "to_int( string_view ) size mismatch" );
  }

  memcpy( &ret, str.data(), sizeof( T ) );

  return ret;
}

ostream& operator<<( ostream& stream, const pretty_print& pp )
{
  auto& runtime = RuntimeStorage::get_instance();
  const bool terminal
    = ( &stream == &cout and isatty( STDOUT_FILENO ) ) or ( &stream == &cerr and isatty( STDERR_FILENO ) );
  if ( pp.name.is_blob() ) {
    const auto view = runtime.user_get_blob( pp.name );
    stream << ( terminal ? "\033[1;34mBlob\033[m" : "Blob" ) << " (" << dec << view.size() << " byte"
           << ( view.size() != 1 ? "s" : "" ) << "): \"";
    for ( const unsigned char ch : view.substr( 0, 32 ) ) {
      if ( ch == '\\' ) {
        stream << "\\\\";
      } else if ( isprint( ch ) ) {
        stream << ch;
      } else {
        stream << "\\x" << hex << setw( 2 ) << setfill( '0' ) << static_cast<unsigned int>( ch );
      }
    }
    if ( view.size() > 32 ) {
      stream << ( terminal ? "\033[2;3m[\xe2\x80\xa6]\033[m" : "[...]" );
    }
    stream << "\"";

    switch ( view.size() ) {
      case sizeof( uint8_t ):
        stream << " (uint8:" << dec << setw( 0 ) << static_cast<unsigned int>( from_int<uint8_t>( view ) ) << ")";
        break;
      case sizeof( uint16_t ):
        stream << " (uint16:" << dec << setw( 0 ) << from_int<uint16_t>( view ) << ")";
        break;
      case sizeof( uint32_t ):
        stream << " (uint32:" << dec << setw( 0 ) << from_int<uint32_t>( view ) << ")";
        break;
      case sizeof( uint64_t ):
        stream << " (uint64:" << dec << setw( 0 ) << from_int<uint64_t>( view ) << ")";
        break;
    }

    stream << "\n";
  } else if ( pp.name.is_tree() ) {
    const auto view = runtime.get_tree( pp.name );
    stream << ( terminal ? "\033[1;32mTree\033[m" : "Tree" ) << " (" << dec << view.size() << " entr"
           << ( view.size() != 1 ? "ies" : "y" ) << "):\n";
    for ( unsigned int i = 0; i < view.size(); ++i ) {
      for ( unsigned int j = 0; j < pp.level + 1; ++j ) {
        stream << "  ";
      }
      stream << to_string( i ) << ". " << pretty_print( view.at( i ), pp.level + 1 );
    }
  } else if ( pp.name.is_thunk() ) {
    Handle encode_name = runtime.get_thunk_encode_name( pp.name );
    stream << ( terminal ? "\033[1;36mThunk\033[m" : "Thunk" ) << ":\n";
    for ( unsigned int i = 0; i < pp.level + 1; ++i ) {
      stream << "  ";
    }
    stream << pretty_print( encode_name, pp.level + 1 );
  } else if ( pp.name.is_tag() ) {
    const auto view = runtime.get_tree( pp.name );
    stream << ( terminal ? "\033[1;32mTag\033[m" : "Tag" ) << " (" << dec << view.size() << " entr"
           << ( view.size() != 1 ? "ies" : "y" ) << "):\n";
    for ( unsigned int i = 0; i < view.size(); ++i ) {
      for ( unsigned int j = 0; j < pp.level + 1; ++j ) {
        stream << "  ";
      }
      stream << to_string( i ) << ". " << pretty_print( view.at( i ), pp.level + 1 );
    }
  } else {
    throw runtime_error( "can't pretty-print object" );
  }
  return stream;
}
