#include <filesystem>
#include <fstream>
#include <glog/logging.h>

#include "base16.hh"
#include "repository.hh"
#include "storage_exception.hh"

using namespace std;
namespace fs = std::filesystem;

Repository::Repository( std::filesystem::path directory )
  : repo_()
{
  namespace fs = std::filesystem;
  auto current_directory = directory;
  while ( not fs::exists( current_directory / ".fix" ) ) {
    if ( not current_directory.has_parent_path() or current_directory == "/" ) {
      throw RepositoryNotFound( directory );
    }
    current_directory = current_directory.parent_path();
  }
  repo_ = directory / ".fix";
  VLOG( 1 ) << "using repository " << repo_;
}

std::unordered_set<Handle<Fix>> Repository::data() const
{
  try {
    std::unordered_set<Handle<Fix>> result;
    for ( const auto& datum : fs::directory_iterator( repo_ / "data" ) ) {
      result.insert( Handle<Fix>::forge( base16::decode( datum.path().filename().string() ) ) );
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

OwnedBlob Repository::get( Handle<Named> name ) const
{
  try {
    Handle<Fix> fix( name );
    VLOG( 1 ) << "loading " << fix.content << " from disk";
    assert( not fix_is_local( fix ) );
    return OwnedBlob::from_file( repo_ / "data" / base16::encode( fix.content ) );
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( name );
  }
}

template<FixTreeType T>
OwnedTree Repository::get( Handle<T> name ) const
{
  try {
    Handle<Fix> fix( name );
    VLOG( 1 ) << "loading " << fix.content << " from disk";
    assert( not fix_is_local( fix ) );
    return OwnedTree::from_file( repo_ / "data" / base16::encode( fix.content ) );
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( name );
  }
}

template OwnedTree Repository::get( Handle<ObjectTree> ) const;
template OwnedTree Repository::get( Handle<ValueTree> ) const;
template OwnedTree Repository::get( Handle<ExpressionTree> ) const;
template OwnedTree Repository::get( Handle<FixTree> ) const;

Handle<Fix> Repository::get( Handle<Relation> relation ) const
{
  try {
    Handle<Fix> fix( relation );
    VLOG( 1 ) << "loading " << fix.content << " from disk";
    assert( not fix_is_local( fix ) );
    return Handle<Fix>::forge( base16::decode(
      fs::read_symlink( repo_ / "relations" / base16::encode( fix.content ) ).filename().string() ) );
  } catch ( std::filesystem::filesystem_error& ) {
    throw HandleNotFound( relation );
  }
}

void Repository::put( Handle<Named> name, OwnedBlob& data )
{
  assert( not fix_is_local( name ) );
  try {
    Handle<Fix> fix( name );
    VLOG( 1 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "data" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    data.to_file( path );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}
template<FixTreeType T>
void Repository::put( Handle<T> name, OwnedTree& data )
{
  assert( not fix_is_local( name ) );
  try {
    Handle<Fix> fix( name );
    VLOG( 1 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "data" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    data.to_file( path );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

void Repository::put( Handle<Relation> relation, Handle<Fix> target )
{
  try {
    assert( not fix_is_local( relation ) );
    assert( not fix_is_local( target ) );
    Handle<Fix> fix( relation );
    VLOG( 1 ) << "writing " << fix.content << " to disk";
    auto path = repo_ / "relations" / base16::encode( fix.content );
    if ( fs::exists( path ) )
      return;
    fs::create_symlink( path, "../data/" + base16::encode( target.content ) );
  } catch ( std::filesystem::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

template void Repository::put( Handle<ObjectTree>, OwnedTree& data );
template void Repository::put( Handle<ValueTree>, OwnedTree& data );
template void Repository::put( Handle<ExpressionTree>, OwnedTree& data );
template void Repository::put( Handle<FixTree>, OwnedTree& data );

Handle<Fix> Repository::labeled( const std::string_view label ) const
{
  try {
    return Handle<Fix>::forge( base16::decode( fs::read_symlink( repo_ / "labels" / label ).filename().string() ) );
  } catch ( fs::filesystem_error& ) {
    throw LabelNotFound( label );
  }
}

void Repository::label( const std::string_view label, Handle<Fix> target )
{
  assert( not fix_is_local( target ) );
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
  assert( not fix_is_local( src ) );
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
  assert( not fix_is_local( src ) );
  if ( dsts.empty() )
    return;
  try {
    auto name = base16::encode( src.content );
    auto dir = repo_ / "pins" / name;
    fs::create_directory( dir );
    for ( const auto& dst : dsts ) {
      assert( not fix_is_local( dst ) );
      fs::create_symlink( dir / base16::encode( dst.content ), "../data/" + name );
    }
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

bool Repository::contains( Handle<Fix> handle ) const
{
  try {
    if ( handle.contains<Relation>() ) {
      return fs::exists( repo_ / "relations" / base16::encode( handle.content ) );
    } else {
      return fs::exists( repo_ / "data" / base16::encode( handle.content ) );
    }
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}

bool Repository::contains( const std::string_view label ) const
{
  try {
    return fs::exists( repo_ / "labels" / label );
  } catch ( fs::filesystem_error& ) {
    throw RepositoryCorrupt( repo_ );
  }
}
