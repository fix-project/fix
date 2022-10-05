#include "name.hh"
#include "object.hh"
#include <iostream>

struct mPair
{
  Name name_;
  size_t ptr_;
  size_t full_hash_;
  bool own_;

  mPair( Name name, uint64_t ptr, const size_t& full_hash, bool own )
    : name_( name )
    , ptr_( ptr )
    , full_hash_( full_hash )
    , own_( own )
  {
  }
};

struct mInfo
{
  uint8_t rh;
  uint8_t hash;

  mInfo()
    : rh( 255 )
    , hash( 0 )
  {
  }
};

// Table size: 2^N
// robin hood psl: 0-254, 255 for empty
// TODO: wrap around for iterator
template<typename Hash>
class Table
{
private:
  std::vector<mPair> pairs_;
  std::vector<mInfo> infos_;

  size_t capacity_;
  size_t N_;

  Hash hash_;

  static constexpr size_t InfoMask = ( 1U << 8 ) - 1U;

  size_t key_to_idx( const size_t& hash_key ) { return ( hash_key >> ( 64 - N_ ) ); }

  uint8_t key_to_hash_info( const size_t& hash_key ) { return static_cast<uint8_t>( hash_key & InfoMask ); }

  size_t find_ptr( Name name )
  {
    auto hash_key = hash_( name );
    size_t idx = key_to_idx( hash_key );
    uint8_t hash_info = key_to_hash_info( hash_key );
    for ( int i = 0; i < 255; i++ ) {
      if ( infos_[idx].rh != 255 && infos_[idx].hash == hash_info ) {
        if ( pairs_[idx].full_hash_ == hash_key ) {
          if ( pairs_[idx].name_ == name ) {
            return pairs_[idx].ptr_;
          }
        }
      } else if ( infos_[idx].rh == 255 || infos_[idx].rh < i ) {
        return 0;
      }
      idx += 1;
      if ( idx == capacity_ ) {
        idx = 0;
      }
    }

    return 0;
  }

  size_t insert_ptr( Name name, uint64_t ptr, bool own )
  {
    auto hash_key = hash_( name );
    mInfo info;
    info.rh = 0;
    info.hash = key_to_hash_info( hash_key );
    size_t idx = key_to_idx( hash_key );
    mPair pair { name, ptr, hash_key, own };

    while ( true ) {
      if ( infos_.at( idx ).rh == 255 ) {
        // If find an empty spot, place the pair here
        infos_[idx] = info;
        pairs_[idx] = std::move( pair );
        return idx;
      } else if ( infos_.at( idx ).rh < info.rh ) {
        // Find a spot with swapping
        std::swap( pair, pairs_[idx] );
        std::swap( info, infos_[idx] );
      }
      idx += 1;
      info.rh += 1;
      if ( idx == capacity_ ) {
        idx = 0;
      }
    }
  }

public:
  Table()
    : pairs_()
    , infos_()
    , capacity_()
    , N_()
    , hash_()
  {
  }

  size_t get_ptr( Name name )
  {
    auto ptr = find_ptr( name );
    return ptr;
  }

  size_t put_ptr( Name name, size_t ptr ) { return insert_ptr( name, ptr, false ); }

  void reserve( size_t N )
  {
    pairs_.reserve( 1UL << N );
    infos_.resize( 1UL << N );
    N_ = N;
    capacity_ = 1UL << N_;
    for ( mInfo& info : infos_ ) {
      info.rh = 255;
    }
  }

  std::string_view get_blob( Name name )
  {
    assert( name.is_blob() );
    auto ptr = find_ptr( name );
    return { ptr, name.get_size() };
  }

  span_view<Name> get_tree( Name name )
  {
    assert( name.is_tree() );
    auto ptr = find_ptr( name );
    return { static_cast<Name*>( ptr ), name.get_size() };
  }

  size_t insert_blob( Name name, Blob&& blob ) { return insert_ptr( name, blob.release(), blob.own() ); }

  size_t insert_tree( Name name, Tree&& tree ) { return insert_ptr( name, tree.release(), tree.own() ); }

  std::vector<mPair>& get_raw_vector() { return pairs_; }
  std::vector<mInfo>& get_raw_info() { return infos_; }

  size_t get_capacity() { return capacity_; }
};
