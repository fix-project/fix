#include <concepts>
#include <filesystem>
#include <glog/logging.h>
#include <memory>

#include "base16.hh"
#include "handle_post.hh"
#include "object.hh"
#include "repository.hh"
#include "storage_exception.hh"

using namespace std;
namespace fs = std::filesystem;

Repository::Repository( std::filesystem::path directory )
  : repo_( find( directory ) )
{
  VLOG( 1 ) << "using repository " << repo_;

  for ( auto h : data() ) {
    h.visit<void>( overload {
      []( Handle<Literal> ) {},
      [&]( Handle<Named> n ) { blobs_.insert( n, true ); },
      [&]( Handle<AnyTree> t ) {
        trees_.insert( t, filesystem::file_size( repo_ / "data" / base16::encode( handle::fix( t ).content ) ) );
      },
      [&]( Handle<Relation> r ) { relations_.insert( r, true ); } } );
  }
}

std::filesystem::path Repository::find( std::filesystem::path directory )
{
  namespace fs = std::filesystem;
  auto current_directory = directory;
  while ( not fs::exists( current_directory / ".fix" ) ) {
    if ( not current_directory.has_parent_path() or current_directory == "/" ) {
      throw RepositoryNotFound( directory );
    }
    current_directory = current_directory.parent_path();
  }
  return current_directory / ".fix";
}

std::unordered_set<Handle<AnyDataType>> Repository::data() const
{
  try {
    std::unordered_set<Handle<AnyDataType>> result;
    for ( const auto& datum : fs::directory_iterator( repo_ / "data" ) ) {
      result.insert(
        handle::data( Handle<Fix>::forge( base16::decode( datum.path().filename().string() ) ) ).value() );
    }

    for ( const auto& relation : relations() ) {
      result.insert( relation );
    }
    return result;
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

std::unordered_set<Handle<Relation>> Repository::relations() const
{
  try {
    std::unordered_set<Handle<Relation>> result;
    for ( const auto& relation : fs::directory_iterator( repo_ / "relations" ) ) {
      result.insert(
        Handle<Fix>::forge( base16::decode( relation.path().filename().string() ) ).unwrap<Relation>() );
    }
    return result;
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

std::unordered_set<std::string> Repository::labels() const
{
  try {
    std::unordered_set<std::string> result;
    for ( const auto& label : fs::directory_iterator( repo_ / "labels" ) ) {
      result.insert( label.path().filename().string() );
    }
    return result;
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

std::unordered_map<Handle<Fix>, std::unordered_set<Handle<Fix>>> Repository::pins() const
{
  try {
    std::unordered_map<Handle<Fix>, std::unordered_set<Handle<Fix>>> result;
    for ( const auto& src : fs::directory_iterator( repo_ / "pins" ) ) {
      std::unordered_set<Handle<Fix>> dsts;
      for ( const auto& dst : fs::directory_iterator( src ) ) {
        dsts.insert( Handle<Fix>::forge( base16::decode( dst.path().filename().string() ) ) );
      }
      result.insert( { Handle<Fix>::forge( base16::decode( src.path().filename().string() ) ), dsts } );
    }
    return result;
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

std::optional<BlobData> Repository::get( Handle<Named> name )
{
  Handle<Fix> fix( name );
  try {
    VLOG( 2 ) << "loading " << fix.content << " from disk";
    assert( not handle::is_local( fix ) );
    return make_shared<OwnedBlob>( repo_ / "data" / base16::encode( fix.content ) );
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( fix );
  }
}

std::optional<TreeData> Repository::get( Handle<AnyTree> name )
{
  auto real_handle = trees_.get_handle( name ).value();
  auto file_name = base16::encode( handle::fix( real_handle ).content );

  try {
    VLOG( 2 ) << "loading " << file_name << " from disk";
    assert( not handle::is_local( name ) );
    return make_shared<OwnedTree>( repo_ / "data" / file_name );
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( handle::fix( name ) );
  }
}

std::optional<TreeData> Repository::get_shallow( Handle<AnyTree> name )
{
  auto t = get( name );

  if ( t.has_value() ) {
    optional<OwnedMutTree> newtree;
    for ( size_t i = 0; i < t.value()->span().size(); i++ ) {
      optional<Handle<Fix>> new_entry;
      t.value()->span()[i].unwrap<Expression>().unwrap<Object>().visit<void>( overload {
        [&]( Handle<Value> x ) {
          x.visit<void>( overload {
            [&]( Handle<Blob> x ) {
              x.visit<void>( overload {
                [&]( Handle<Named> x ) { new_entry = Handle<BlobRef>( x ); },
                []( Handle<Literal> ) {},
              } );
            },
            [&]( Handle<ValueTree> x ) { new_entry = x.into<ValueTreeRef>( trees_.get( x ).value() ); },
            []( Handle<BlobRef> ) {},
            []( Handle<ValueTreeRef> ) {},
          } );
        },
        [&]( Handle<ObjectTree> x ) { new_entry = x.into<ObjectTreeRef>( trees_.get( x ).value() ); },
        []( Handle<Thunk> ) {},
        []( Handle<ObjectTreeRef> ) {},
      } );

      if ( new_entry.has_value() ) {
        if ( !newtree.has_value() ) {
          newtree = OwnedMutTree::allocate( t.value()->span().size() );
          std::copy( t.value()->span().begin(), t.value()->span().end(), newtree.value().span().begin() );
        }

        newtree.value()[i] = new_entry.value();
      }
    }

    if ( newtree.has_value() ) {
      return make_shared<OwnedTree>( std::move( newtree.value() ) );
    }
  }

  return t;
}

std::optional<Handle<Object>> Repository::get( Handle<Relation> relation )
{
  Handle<Fix> fix( relation );
  try {
    VLOG( 2 ) << "loading " << fix.content << " from disk";
    assert( not handle::is_local( fix ) );
    return Handle<Fix>::forge(
             base16::decode(
               fs::read_symlink( repo_ / "relations" / base16::encode( fix.content ) ).filename().string() ) )
      .unwrap<Expression>()
      .unwrap<Object>();
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( fix );
  }
}

void Repository::put( Handle<Named> name, BlobData data )
{
  assert( not handle::is_local( name ) );
  try {
    Handle<Fix> fix( name );
    VLOG( 2 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "data" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    blobs_.insert( name, true );
    data->to_file( path );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

void Repository::put( Handle<AnyTree> name, TreeData data )
{
  assert( not handle::is_local( name ) );
  try {
    auto fix = name.visit<Handle<Fix>>( []( const auto x ) { return x; } );
    VLOG( 2 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "data" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    trees_.insert( name, true );
    data->to_file( path );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

void Repository::put_shallow( Handle<AnyTree>, TreeData )
{
  throw std::runtime_error( "Unimplemented" );
}

void Repository::put( Handle<Relation> relation, Handle<Object> target )
{
  try {
    assert( not handle::is_local( relation ) );
    assert( not handle::is_local( target ) );
    Handle<Fix> fix( relation );
    VLOG( 2 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "relations" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    VLOG( 2 ) << "linking to " << target.content;
    relations_.insert( relation, true );
    fs::create_symlink( "../data/" + base16::encode( target.content ), path );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

Handle<Fix> Repository::labeled( const std::string_view label )
{
  try {
    return Handle<Fix>::forge( base16::decode( fs::read_symlink( repo_ / "labels" / label ).filename().string() ) );
  } catch ( fs::filesystem_error& ) {
    throw LabelNotFound( label );
  }
}

void Repository::label( const std::string_view label, Handle<Fix> target )
{
  assert( not handle::is_local( target ) );
  try {
    auto path = repo_ / "labels" / label;
    if ( fs::exists( path ) )
      fs::remove( path );
    fs::create_symlink( "../data/" + base16::encode( target.content ), path );
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

std::unordered_set<Handle<Fix>> Repository::pinned( Handle<Fix> src ) const
{
  assert( not handle::is_local( src ) );
  try {
    std::unordered_set<Handle<Fix>> dsts;
    for ( const auto& dst : fs::directory_iterator( repo_ / "pins" / base16::encode( src.content ) ) ) {
      dsts.insert( Handle<Fix>::forge( base16::decode( dst.path().filename().string() ) ) );
    }
    return dsts;
  } catch ( fs::filesystem_error& ) {
    return {};
  }
}

void Repository::pin( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts )
{
  assert( not handle::is_local( src ) );
  if ( dsts.empty() )
    return;
  try {
    auto name = base16::encode( src.content );
    auto dir = repo_ / "pins" / name;
    fs::create_directory( dir );
    for ( const auto& dst : dsts ) {
      assert( not handle::is_local( dst ) );
      fs::create_symlink( dir / base16::encode( dst.content ), "../data/" + name );
    }
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

bool Repository::contains( Handle<Named> handle )
{
  return blobs_.contains( handle );
}

bool Repository::contains( Handle<AnyTree> handle )
{
  return trees_.contains( handle );
}

bool Repository::contains_shallow( Handle<AnyTree> handle )
{
  return contains( handle );
}

bool Repository::contains( Handle<Relation> handle )
{
  return relations_.contains( handle );
}

std::optional<Handle<AnyTree>> Repository::contains( Handle<AnyTreeRef> handle )
{
  auto tmp_tree = Handle<AnyTree>::forge( handle.content );

  auto entry = trees_.get_handle( tmp_tree );

  if ( !entry.has_value() ) {
    return {};
  }

  auto tagged = handle.visit<bool>( []( auto h ) { return h.is_tag(); } );

  if ( tagged ) {
    return entry->visit<Handle<AnyTree>>( []( auto h ) { return h.tag(); } );
  } else {
    return entry;
  }
}

std::optional<Handle<AnyTree>> Repository::get_handle( Handle<AnyTree> name )
{
  return trees_.get_handle( name );
}

#if 0
bool Repository::contains( Handle<Fix> handle )
{
  try {
    if ( handle.contains<Relation>() ) {
      return fs::exists( repo_ / "data"
                         / base16::encode( handle.visit<Handle<Fix>>( [&]( auto x ) { return x; } ).content ) );
    } else {
      return fs::exists(
        repo_ / "data"
        / base16::encode( handle::data( handle ).visit<Handle<Fix>>( []( auto x ) { return x; } ).content ) );
    }
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}
#endif

bool Repository::contains( const std::string_view label )
{
  try {
    return fs::is_symlink( repo_ / "labels" / label );
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}
/* vector<string> RuntimeStorage::get_friendly_names( Handle<Fix> handle ) */
/* { */
/*   vector<string> names; */
/*   for ( const auto& [l, h] : labels_ ) { */
/*     if ( h == handle ) { */
/*       names.push_back( l ); */
/*     } */
/*   } */
/*   return names; */
/* } */

/* string RuntimeStorage::get_encoded_name( Handle<Fix> handle ) */
/* { */
/*   return base16::encode( handle.content ); */
/* } */

/* string RuntimeStorage::get_short_name( Handle<Fix> handle ) */
/* { */
/*   handle = canonicalize( handle ); */
/*   auto encoded = get_encoded_name( handle ); */
/*   return encoded.substr( 0, 7 ); */
/* } */

/* Handle<Fix> RuntimeStorage::get_handle( const std::string_view short_name ) */
/* { */
/*   std::optional<Handle<Fix>> candidate; */
/*   for ( const auto& [handle, data] : canonical_storage_ ) { */
/*     (void)data; */
/*     std::string name = base16::encode( handle.content ); */
/*     if ( name.rfind( short_name, 0 ) == 0 ) { */
/*       if ( candidate ) */
/*         throw AmbiguousReference( short_name ); */
/*       candidate = handle; */
/*     } */
/*   } */
/*   if ( candidate ) */
/*     return *candidate; */
/*   throw ReferenceNotFound( short_name ); */
/* } */

/* string RuntimeStorage::get_display_name( Handle<Fix> handle ) */
/* { */
/*   vector<string> friendly_names = get_friendly_names( handle ); */
/*   string fullname = get_short_name( handle ); */
/*   if ( not friendly_names.empty() ) { */
/*     fullname += " ("; */
/*     for ( const auto& name : friendly_names ) { */
/*       fullname += name + ", "; */
/*     } */
/*     fullname = fullname.substr( 0, fullname.size() - 2 ); */
/*     fullname += ")"; */
/*   } */
/*   return fullname; */
/* } */

Handle<Fix> Repository::lookup( const std::string_view ref )
{
  if ( ref.size() == 64 ) {
    auto handle = Handle<Fix>::forge( base16::decode( ref ) );
    if ( handle::data( handle ).has_value() ) {
      if ( handle::data( handle )->visit<bool>( overload {
             [&]( Handle<Literal> ) { return true; },
             [&]( auto x ) { return contains( x ); },
           } ) )
        return handle;
    } else {
      return handle;
    }
  }

  try {
    return labeled( ref );
  } catch ( LabelNotFound& ) {}

  std::optional<Handle<AnyDataType>> candidate;
  for ( const auto& handle : data() ) {
    std::string name = base16::encode( handle.content );
    if ( name.rfind( ref, 0 ) == 0 ) {
      if ( candidate )
        throw AmbiguousReference( ref );
      candidate = handle::data( handle::fix( handle ) ).value();
    }
  }
  if ( candidate )
    return handle::fix( *candidate );

  if ( fs::exists( ref ) ) {
    try {
      return create( std::make_shared<OwnedBlob>( ref ) );
    } catch ( fs::filesystem_error& ) {}
  }
  throw ReferenceNotFound( ref );
}
