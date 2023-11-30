#include <charconv>
#include <stdexcept>

#include "tester-utils.hh"

#include "base16.hh"

using namespace std;

static bool deserialized = false;

template<std::integral T>
T to_int( const std::string_view str );
template<std::integral T>
T from_int( const std::string_view str );

Handle make_blob( std::string_view s )
{
  auto& rt = Runtime::get_instance();
  if ( s.size() < 31 ) {
    return Handle( s );
  } else {
    return rt.storage().add_blob( OwnedBlob::copy( s ) );
  }
}

template<typename T>
Handle make_blob( T t ) requires std::is_integral_v<T>
{
  std::string_view s { (const char*)&t, sizeof( t ) };
  return Handle( s );
}

Handle get_full_name( string_view short_name )
{
  char prefix = short_name[0];
  short_name.remove_prefix( 1 );

  auto fix_dir = Runtime::get_instance().storage().get_fix_repo();
  for ( const auto& file : filesystem::directory_iterator( fix_dir / "objects" ) ) {
    if ( file.path().filename().string().starts_with( short_name ) ) {
      Handle full = base16::decode( file.path().filename().string() );

      if ( prefix == 'G' and !full.is_tag() ) {
        continue;
      }

      if ( prefix == 'B' and !full.is_blob() ) {
        throw std::runtime_error( "Object exists but it is not a Blob." );
      }

      if ( ( prefix == 'T' or prefix == 'K' ) and !full.is_tree() ) {
        throw std::runtime_error( "Object exists but it is not a Tree/Thunk." );
      }

      if ( prefix == 'K' ) {
        full = full.as_thunk();
      }

      return full;
    }
  }

  throw std::runtime_error( "Short name does not exist." );
}

/**
 * Adds the args to RuntimeStorage, loading files and creating objects as necessary.
 * The contents of @p open_files must outlive this RuntimeStorage instance.
 */
Handle parse_args( span_view<char*>& args, vector<ReadOnlyFile>& open_files, bool deserialize )
{
  auto& rt = Runtime::get_instance();
  auto& storage = Runtime::get_instance().storage();

  if ( deserialize ) {
    rt.deserialize();
    deserialized = true;
  }

  if ( args.empty() ) {
    throw runtime_error( "not enough args" );
  }

  const string_view str { args[0] };

  if ( str.starts_with( "file:" ) ) {
    std::filesystem::path file( string( str.substr( 5 ) ) );
    args.remove_prefix( 1 );
    return storage.add_blob( OwnedBlob::from_file( file ) );
  }

  if ( str.starts_with( "compile:" ) ) {
    std::filesystem::path file( string( str.substr( 8 ) ) );
    args.remove_prefix( 1 );
    if ( !deserialized ) {
      rt.deserialize();
      deserialized = true;
    }
    OwnedMutTree compile_tree = OwnedMutTree::allocate( 3 );
    compile_tree.at( 0 ) = Handle( "unused" );
    compile_tree.at( 1 ) = COMPILE_ENCODE;
    Handle blob = storage.add_blob( OwnedBlob::from_file( file ) );
    compile_tree.at( 2 ) = blob;
    return storage.add_tree( std::move( compile_tree ) ).as_thunk();
  }

  if ( str.starts_with( "ref:" ) ) {
    if ( !deserialized ) {
      rt.deserialize();
      deserialized = true;
    }
    args.remove_prefix( 1 );
    auto ref = storage.get_ref( str.substr( 4 ) );
    if ( ref ) {
      return *ref;
    } else {
      throw runtime_error( string( "Ref not found: " ).append( str.substr( 4 ) ) );
    }
  }

  if ( str.starts_with( "name:" ) ) {
    if ( !deserialized ) {
      rt.deserialize();
      deserialized = true;
    }
    args.remove_prefix( 1 );
    return base16::decode( str.substr( 5 ) );
  }

  if ( str.starts_with( "short-name:" ) ) {
    if ( !deserialized ) {
      rt.deserialize();
      deserialized = true;
    }
    args.remove_prefix( 1 );
    return get_full_name( str.substr( 11 ) );
  }

  if ( str.starts_with( "string:" ) ) {
    args.remove_prefix( 1 );
    return make_blob( str.substr( 7 ) );
  }

  if ( str.starts_with( "uint8:" ) ) {
    args.remove_prefix( 1 );
    return make_blob( to_int<uint8_t>( str.substr( 6 ) ) );
  }

  if ( str.starts_with( "uint16:" ) ) {
    args.remove_prefix( 1 );
    return make_blob( to_int<uint16_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "uint32:" ) ) {
    args.remove_prefix( 1 );
    return make_blob( to_int<uint32_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "uint64:" ) ) {
    args.remove_prefix( 1 );
    return make_blob( to_int<uint64_t>( str.substr( 7 ) ) );
  }

  if ( str.starts_with( "tree:" ) ) {
    const uint32_t tree_size = to_int<uint32_t>( str.substr( 5 ) );
    args.remove_prefix( 1 );
    if ( args.size() < tree_size ) {
      throw runtime_error( "not enough args to make Tree of length " + to_string( tree_size ) );
    }

    OwnedMutTree the_tree = OwnedMutTree::allocate( tree_size );
    for ( uint32_t i = 0; i < tree_size; ++i ) {
      the_tree[i] = parse_args( args, open_files );
    }
    return storage.add_tree( std::move( the_tree ) );
  }

  if ( str.starts_with( "thunk:" ) ) {
    args.remove_prefix( 1 );
    const string_view str1 { args[0] };
    if ( !str1.starts_with( "tree:" ) ) {
      throw runtime_error( "thunk not refering a tree" );
    }

    Handle tree_name = parse_args( args, open_files );
    return tree_name.as_thunk();
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
T from_int( const std::span<const char> str )
{
  T ret;
  if ( sizeof( T ) != str.size() ) {
    throw runtime_error( "to_int( string_view ) size mismatch" );
  }

  memcpy( &ret, str.data(), sizeof( T ) );

  return ret;
}

ostream& operator<<( ostream& stream, const deep_pretty_print& pp )
{
  auto& runtime = Runtime::get_instance().storage();
  const bool terminal
    = ( &stream == &cout and isatty( STDOUT_FILENO ) ) or ( &stream == &cerr and isatty( STDERR_FILENO ) );
  if ( pp.name.is_blob() ) {
    const auto view = runtime.get_blob( pp.name );
    stream << ( terminal ? "\033[1;34mBlob\033[m" : "Blob" ) << " (" << dec << view.size() << " byte"
           << ( view.size() != 1 ? "s" : "" ) << "): \"";
    for ( const unsigned char ch : view.size() < 32 ? view : view.subspan( 0, 32 ) ) {
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
      stream << to_string( i ) << ". " << deep_pretty_print( view[i], pp.level + 1 );
    }
  } else if ( pp.name.is_thunk() ) {
    Handle encode_name = pp.name.as_tree();
    stream << ( terminal ? "\033[1;36mThunk\033[m" : "Thunk" ) << ":\n";
    for ( unsigned int i = 0; i < pp.level + 1; ++i ) {
      stream << "  ";
    }
    stream << deep_pretty_print( encode_name, pp.level + 1 );
  } else if ( pp.name.is_tag() ) {
    const auto view = runtime.get_tree( pp.name );
    stream << ( terminal ? "\033[1;32mTag\033[m" : "Tag" ) << " (" << dec << view.size() << " entr"
           << ( view.size() != 1 ? "ies" : "y" ) << "):\n";
    for ( unsigned int i = 0; i < view.size(); ++i ) {
      for ( unsigned int j = 0; j < pp.level + 1; ++j ) {
        stream << "  ";
      }
      stream << to_string( i ) << ". " << deep_pretty_print( view[i], pp.level + 1 );
    }
  } else {
    throw runtime_error( "can't pretty-print object" );
  }
  return stream;
}

ostream& operator<<( ostream& stream, const pretty_print& pp )
{
  auto& runtime = Runtime::get_instance().storage();
  const bool terminal
    = ( &stream == &cout and isatty( STDOUT_FILENO ) ) or ( &stream == &cerr and isatty( STDERR_FILENO ) );
  if ( pp.name.is_blob() ) {
    const auto view = runtime.get_blob( pp.name );
    stream << ( terminal ? "\033[1;34mBlob\033[m" : "Blob" ) << " (" << dec << view.size() << " byte"
           << ( view.size() != 1 ? "s" : "" ) << "): \"";
    for ( const unsigned char ch : view.size() < 32 ? view : view.subspan( 0, 32 ) ) {
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
      stream << "  " << to_string( i ) << ". " << runtime.get_display_name( view[i] ) << endl;
    }
  } else if ( pp.name.is_thunk() ) {
    Handle encode_name = pp.name.as_tree();
    stream << ( terminal ? "\033[1;36mThunk\033[m" : "Thunk" ) << ":\n";
    stream << runtime.get_display_name( encode_name );
  } else if ( pp.name.is_tag() ) {
    const auto view = runtime.get_tree( pp.name );
    stream << ( terminal ? "\033[1;32mTag\033[m" : "Tag" ) << " (" << dec << view.size() << " entr"
           << ( view.size() != 1 ? "ies" : "y" ) << "):\n";
    for ( unsigned int i = 0; i < view.size(); ++i ) {
      stream << "  " << to_string( i ) << ". " << runtime.get_display_name( view[i] ) << endl;
    }
  } else {
    throw runtime_error( "can't pretty-print object" );
  }
  return stream;
}
