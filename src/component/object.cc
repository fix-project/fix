#include "object.hh"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <glog/logging.h>

template<typename S>
Owned<S>::Owned( S span, AllocationType allocation_type )
  : span_( span )
  , allocation_type_( allocation_type )
{}

template<typename S>
Owned<S>::Owned( std::filesystem::path path ) requires std::is_const_v<element_type>
  : span_()
  , allocation_type_( AllocationType::Mapped )
{
  VLOG( 2 ) << "mapping " << path << " as read-only";
  size_t size = std::filesystem::file_size( path );
  int fd = open( path.c_str(), O_RDONLY );
  CHECK( fd >= 0 );
  void* p = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
  CHECK( p );
  close( fd );
  span_ = { reinterpret_cast<pointer>( p ), size / sizeof( element_type ) };
}

template<typename S>
Owned<S>::Owned( size_t size, AllocationType type ) requires( not std::is_const_v<element_type> )
  : span_()
  , allocation_type_( type )
{
  switch ( type ) {
    case AllocationType::Allocated: {
      void* p = aligned_alloc( std::alignment_of<element_type>(), size * sizeof( element_type ) );
      CHECK( p );
      span_ = {
        reinterpret_cast<pointer>( p ),
        size,
      };
      VLOG( 2 ) << "allocated " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
                << reinterpret_cast<void*>( span_.data() );
      return;
    }
    case AllocationType::Mapped: {
      void* p = mmap( 0, size * sizeof( element_type ), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0 );
      if ( p == (void*)-1 ) {
        perror( "mmap" );
        CHECK( p != (void*)-1 );
      }
      CHECK( p );
      span_ = {
        reinterpret_cast<pointer>( p ),
        size,
      };
      VLOG( 2 ) << "mapped " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
                << reinterpret_cast<void*>( span_.data() );
      return;
    }
    case AllocationType::Static:
      CHECK( false );
  }
}

template<typename S>
void Owned<S>::to_file( const std::filesystem::path path ) requires std::is_const_v<element_type>
{
  VLOG( 2 ) << "writing " << path << " to disk";
  size_t bytes = span_.size_bytes();
  CHECK( not std::filesystem::exists( path ) );
  int fd = open( path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR );
  CHECK( fd >= 0 );
  int status = ftruncate( fd, bytes );
  CHECK( status == 0 );
  VLOG( 2 ) << "resized " << path << " to " << bytes;
  VLOG( 2 ) << "mmapping " << path;
  void* p = mmap( NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
  if ( p == (void*)-1 ) {
    perror( "mmap" );
    CHECK( p != (void*)-1 );
  }
  CHECK( p );
  close( fd );
  VLOG( 2 ) << "memcpying " << reinterpret_cast<const void*>( span_.data() ) << " to " << p;
  memmove( p, const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ), bytes );
  VLOG( 2 ) << "mprotecting " << path;
  mprotect( p, bytes, PROT_READ );
  VLOG( 2 ) << "writing to disk " << path;
  msync( p, bytes, MS_SYNC );

  // Transfer the old span to another object, which will deallocate it.
  auto discard = Owned { span_, allocation_type_ };

  span_ = { reinterpret_cast<pointer>( p ), span_.size() };
  allocation_type_ = AllocationType::Mapped;
}

template<typename S>
Owned<S>::Owned( Owned<S>&& other )
  : span_( other.span_ )
  , allocation_type_( other.allocation_type_ )
{
  other.leak();
}

template<typename S>
Owned<S>::Owned( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>
  : span_( original.span() )
  , allocation_type_( original.allocation_type() )
{
  original.leak();
  if ( allocation_type_ == AllocationType::Mapped ) {
    VLOG( 2 ) << "Setting allocation at " << reinterpret_cast<const void*>( span_.data() ) << " "
              << "(" << span_.size_bytes() << " bytes) to RO.";
    int result = mprotect( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ),
                           span_.size_bytes(),
                           PROT_READ | PROT_EXEC );
    if ( result != 0 ) {
      perror( "mprotect" );
      CHECK( result == 0 );
    }
  }
};

template<typename S>
Owned<S>& Owned<S>::operator=( Owned<std::span<value_type>>&& original ) requires std::is_const_v<element_type>
{
  span_ = original.span();
  allocation_type_ = original.allocation_type();
  original.leak();
  if ( allocation_type_ == AllocationType::Mapped ) {
    VLOG( 2 ) << "Setting allocation at " << reinterpret_cast<const void*>( span_.data() ) << " "
              << "(" << span_.size_bytes() << " bytes) to RO.";
    int result = mprotect( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ),
                           span_.size_bytes(),
                           PROT_READ | PROT_EXEC );
    if ( result != 0 ) {
      perror( "mprotect" );
      CHECK( result == 0 );
    }
  }
  return *this;
}

template<typename S>
void Owned<S>::leak()
{
  span_ = { reinterpret_cast<S::value_type*>( 0 ), 0 };
  allocation_type_ = AllocationType::Static;
}

template<typename S>
Owned<S>& Owned<S>::operator=( Owned&& other )
{
  this->span_ = other.span_;
  this->allocation_type_ = other.allocation_type_;
  other.leak();
  return *this;
}

template<typename S>
auto Owned<S>::operator[]( size_t index ) -> element_type&
{
  CHECK( index < span_.size() );
  return span_[index];
}

template<typename S>
Owned<S>::~Owned()
{
  switch ( allocation_type_ ) {
    case AllocationType::Static:
      break;
    case AllocationType::Allocated:
      VLOG( 2 ) << "freeing " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
      free( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ) );
      break;
    case AllocationType::Mapped:
      VLOG( 2 ) << "unmapping " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
      munmap( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ), span_.size() );
      break;
  }
  leak();
}

template class Owned<BlobSpan>;
template class Owned<TreeSpan>;
template class Owned<MutBlobSpan>;
template class Owned<MutTreeSpan>;
