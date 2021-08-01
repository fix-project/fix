#include <iostream>

#include "runtimestorage.hh"

#include "invocation.hh"

using namespace std;

uint64_t Invocation::next_invocation_id_ = 0;

void Invocation::attach_input( uint32_t input_index, uint32_t mem_index )
{
  // if ( input_index >= input_count_ )
  // {
  // throw out_of_range ( "Input does not exist." );
  // }

  Name strict_input = RuntimeStorage::getInstance().getTree( encode_name_ ).at( 1 );
  Name input_name = RuntimeStorage::getInstance().getTree( strict_input ).at( input_index );

  input_mems[mem_index] = input_name;
}

template<typename T>
uint32_t Invocation::get_i32( uint32_t mem_index, uint32_t ofst )
{
  switch ( input_mems[mem_index].getContentType() ) {
    case ContentType::Blob:
    case ContentType::Unknown: {
      uint32_t res = *(const T*)( RuntimeStorage::getInstance().getBlob( input_mems[mem_index] ).data() + ofst );
      return res;
    }

    default:
      throw runtime_error( "No write access." );
  }
}

void Invocation::attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type )
{
  if ( output_index >= output_count_ ) {
    output_count_ = output_index + 1;
  }

  outputs.push_back( make_shared<OutputTemp>( output_index, output_type ) );
  output_mems[mem_index] = outputs.back();

  return;
}

void Invocation::attach_output_child( uint32_t parent_mem_index,
                                      uint32_t child_index,
                                      uint32_t child_mem_index,
                                      uint32_t output_type )
{
  shared_ptr<OutputTemp> parent_mem = output_mems[parent_mem_index];

  switch ( parent_mem->content_type_ ) {
    case TREE:
      break;

    default:
      throw runtime_error( "Cannot add child to non-Tree output." );
  }

  if ( child_index >= get<InProgressTree>( parent_mem->content_ ) ) {
    parent_mem->content_.emplace<InProgressTree>( child_index + 1 );
  }

  outputs.push_back( make_shared<OutputTemp>( parent_mem->path_, child_index, output_type ) );
  output_mems[child_mem_index] = outputs.back();

  return;
}

template<typename T>
void Invocation::store_i32( uint32_t mem_index, uint32_t content )
{
  shared_ptr<OutputTemp> output_mem = output_mems[mem_index];
  switch ( output_mem->content_type_ ) {
    case BLOB: {
      T content_T = (T)content;
      get<InProgressBlob>( output_mem->content_ ).append( (char*)&content_T, sizeof( T ) );
      return;
    }

    default:
      throw runtime_error( "Cannot write to non-Blob output." );
  }
}

void Invocation::set_encode( uint32_t mem_index, uint32_t encode_mem_index )
{
  shared_ptr<OutputTemp> output_mem = output_mems[mem_index];

  switch ( output_mem->content_type_ ) {
    case THUNK:
      break;

    default:
      throw runtime_error( "Cannot set encode to a non-Thunk output." );
  }

  shared_ptr<OutputTemp> encode_mem = output_mems[encode_mem_index];
  get<InProgressThunk>( output_mem->content_ ).encode_path_ = encode_mem->path_;

  return;
}

void Invocation::add_path( uint32_t mem_index, uint32_t path_index )
{
  shared_ptr<OutputTemp> output_mem = output_mems[mem_index];

  switch ( output_mem->content_type_ ) {
    case THUNK:
      break;

    default:
      throw runtime_error( "Cannot set encode to a non-Thunk output." );
  }

  get<InProgressThunk>( output_mem->content_ ).path_.push_back( path_index );
  return;
}

void Invocation::move_lazy_input( uint32_t mem_index, uint32_t child_index, uint32_t lazy_input_index )
{
  shared_ptr<OutputTemp> output_mem = output_mems[mem_index];

  switch ( output_mem->content_type_ ) {
    case TREE:
      break;

    default:
      throw runtime_error( "Cannot move lazy input to non-Tree output." );
  }

  if ( child_index >= get<InProgressTree>( output_mem->content_ ) ) {
    output_mem->content_.emplace<InProgressTree>( child_index + 1 );
  }

  Name input_name;
  if ( RuntimeStorage::getInstance().getTree( encode_name_ ).size() < 3 ) {
    input_name
      = Name( RuntimeStorage::getInstance().name_to_program_.at( program_name_ ).getDeps().at( lazy_input_index ),
              NameType::Canonical,
              ContentType::Blob );
  } else {
    Name lazy_input = RuntimeStorage::getInstance().getTree( encode_name_ ).at( 2 );
    input_name = RuntimeStorage::getInstance().getTree( lazy_input ).at( lazy_input_index );
  }
  outputs.push_back( make_shared<OutputTemp>( output_mem->path_, child_index, input_name ) );
  return;
}

void Invocation::add_to_storage()
{
  for ( auto output : outputs ) {
    switch ( output->content_type_ ) {
      case BLOB: {
        // cout << "Adding blob at " << Name( encode_name_, output->path_, ContentType::Blob ) << endl;
        RuntimeStorage::getInstance().thunk_to_blob_.insert_or_assign(
          Name( encode_name_, output->path_, ContentType::Blob ),
          RuntimeStorage::getInstance().addBlob( move( get<InProgressBlob>( output->content_ ) ) ) );
      } break;

      case TREE: {
        Tree tree_res;
        // cout << "Adding tree at " << Name( encode_name_, output->path_, ContentType::Tree ) << endl;
        for ( size_t i = 0; i < get<InProgressTree>( output->content_ ); i++ ) {
          vector<size_t> child_path = output->path_;
          child_path.push_back( i );
          tree_res.push_back( Name( encode_name_, child_path, ContentType::Unknown ) );
        }
        RuntimeStorage::getInstance().name_to_tree_.put( Name( encode_name_, output->path_, ContentType::Tree ),
                                                         move( tree_res ) );
      } break;

      case THUNK: {
        // cout << "Adding thunk at " << Name ( encode_name_, output->path_, ContentType::Thunk ) << endl;
        Thunk res( Name( encode_name_, get<InProgressThunk>( output->content_ ).encode_path_, ContentType::Tree ),
                   get<InProgressThunk>( output->content_ ).path_ );
        RuntimeStorage::getInstance().name_to_thunk_.put( Name( encode_name_, output->path_, ContentType::Thunk ),
                                                          move( res ) );
      } break;

      case NAME:
        // cout << "Adding name at " << Name ( encode_name_, output->path_, ContentType::Blob ) << endl;
        RuntimeStorage::getInstance().thunk_to_blob_.insert_or_assign(
          Name( encode_name_, output->path_, ContentType::Blob ), get<Name>( output->content_ ) );
        break;
    }
  }
  outputs = vector<shared_ptr<OutputTemp>>();
  return;
}

uint32_t Invocation::get_int( uint32_t mem_index, uint32_t ofst )
{
  return get_i32<int>( mem_index, ofst );
}

void Invocation::store_int( uint32_t mem_index, uint32_t content )
{
  return store_i32<int>( mem_index, content );
}

uint32_t Invocation::mem_copy( uint32_t mem_index, uint32_t ofst, uint8_t * mem_loc, uint32_t iovs_len )
{
  switch ( input_mems[mem_index].getContentType() ) {
    case ContentType::Blob:
    case ContentType::Unknown: {
      uint32_t len = max( static_cast<uint32_t>( RuntimeStorage::getInstance().getBlob( input_mems[mem_index] ).size() ) - ofst, iovs_len);
      memcpy( mem_loc, RuntimeStorage::getInstance().getBlob( input_mems[mem_index] ).data() + ofst, len );
      return len;
    }

    default:
      throw runtime_error( "No write access." );
  }
}

