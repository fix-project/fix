#include "object.hh"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <glog/logging.h>

template<any_span S>
Owned<S>::Owned( S span, AllocationType allocation_type )
  : span_( span )
  , allocation_type_( allocation_type )
{}

template<any_span S>
Owned<S> Owned<S>::allocate( size_t size ) requires( not std::is_const_v<element_type> )
{
  void* p = aligned_alloc( std::alignment_of<element_type>(), size * sizeof( element_type ) );
  CHECK( p );
  span_type span = {
    reinterpret_cast<pointer>( p ),
    size,
  };
  VLOG( 1 ) << "allocated " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
            << reinterpret_cast<void*>( span.data() );
  return {
    span,
    AllocationType::Allocated,
  };
}

template<any_span S>
Owned<S> Owned<S>::map( size_t size ) requires( not std::is_const_v<element_type> )
{
  void* p = mmap( 0, size * sizeof( element_type ), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0 );
  if ( p == (void*)-1 ) {
    perror( "mmap" );
    CHECK( p != (void*)-1 );
  }
  CHECK( p );
  span_type span = {
    reinterpret_cast<pointer>( p ),
    size,
  };
  VLOG( 1 ) << "mapped " << size << " elements (" << size * sizeof( element_type ) << " bytes) at "
            << reinterpret_cast<void*>( span.data() );
  return {
    span,
    AllocationType::Mapped,
  };
}

template<any_span S>
Owned<S> Owned<S>::claim_static( S span )
{
  VLOG( 1 ) << "claimed " << span.size_bytes() << " static bytes at "
            << reinterpret_cast<const void*>( span.data() );
  return Owned { span, AllocationType::Static };
}

template<any_span S>
Owned<S> Owned<S>::claim_allocated( S span )
{
  VLOG( 1 ) << "claimed " << span.size_bytes() << " allocated bytes at "
            << reinterpret_cast<const void*>( span.data() );
  return Owned { span, AllocationType::Allocated };
}

template<any_span S>
Owned<S> Owned<S>::claim_mapped( S span )
{
  VLOG( 1 ) << "claimed " << span.size_bytes() << " mapped bytes at "
            << reinterpret_cast<const void*>( span.data() );
  return Owned { span, AllocationType::Mapped };
}

template<any_span S>
Owned<S> Owned<S>::from_file( const std::filesystem::path path ) requires std::is_const_v<element_type>
{
  VLOG( 1 ) << "mapping " << path << " as read-only";
  size_t size = std::filesystem::file_size( path );
  int fd = open( path.c_str(), O_RDONLY );
  CHECK( fd >= 0 );
  void* p = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
  CHECK( p );
  close( fd );
  return claim_mapped( { reinterpret_cast<pointer>( p ), size / sizeof( element_type ) } );
}

template<any_span S>
Owned<S> Owned<S>::copy( std::span<element_type> data )
{
  element_type* a = (element_type*)malloc( data.size_bytes() );
  memcpy( (void*)a, (void*)data.data(), data.size_bytes() );
  return claim_allocated( { a, data.size() } );
}

template<any_span S>
void Owned<S>::to_file( const std::filesystem::path path ) requires std::is_const_v<element_type>
{
  VLOG( 1 ) << "writing " << path << " to disk";
  size_t bytes = span_.size_bytes();
  CHECK( not std::filesystem::exists( path ) );
  int fd = open( path.c_str(), O_RDWR | O_CREAT | O_TRUNC, O_RDWR );
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

template<any_span S>
Owned<S>::Owned( Owned<S>&& other )
  : span_( other.span_ )
  , allocation_type_( other.allocation_type_ )
{
  other.leak();
}

template<any_span S>
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

template<any_span S>
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

template<any_span S>
void Owned<S>::leak()
{
  span_ = { reinterpret_cast<S::value_type*>( 0 ), 0 };
  allocation_type_ = AllocationType::Static;
}

template<any_span S>
Owned<S>& Owned<S>::operator=( Owned&& other )
{
  this->span_ = other.span_;
  this->allocation_type_ = other.allocation_type_;
  other.leak();
  return *this;
}

template<any_span S>
auto Owned<S>::operator[]( size_t index ) -> element_type&
{
  CHECK( index < span_.size() );
  return span_[index];
}

template<any_span S>
Owned<S>::~Owned()
{
  switch ( allocation_type_ ) {
    case AllocationType::Static:
      break;
    case AllocationType::Allocated:
      VLOG( 1 ) << "freeing " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
      free( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ) );
      break;
    case AllocationType::Mapped:
      VLOG( 1 ) << "unmapping " << span_.size() << " bytes at " << reinterpret_cast<const void*>( span_.data() );
      munmap( const_cast<void*>( reinterpret_cast<const void*>( span_.data() ) ), span_.size() );
      break;
  }
  leak();
}

template class Owned<MutBlob>;
template class Owned<MutTree>;
template class Owned<Blob>;
template class Owned<Tree>;
