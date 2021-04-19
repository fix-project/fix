#include "runtimestorage.hh"

#include "invocation.hh"

using namespace std;

uint64_t Invocation::next_invocation_id_ = 0;

void Invocation::attach_input( uint32_t input_index, uint32_t mem_index )
{
  if ( input_index >= input_count_ )
  {
    throw out_of_range ( "Input does not exist." );
  }

  Name strict_input = RuntimeStorage::getInstance().getTree( encode_name_ ).at(1);
  Name input_name = RuntimeStorage::getInstance().getTree( strict_input ).at( input_index );

  input_mems[mem_index] = input_name;
}

template<typename T>
uint32_t Invocation::get_i32( uint32_t mem_index, uint32_t ofst )
{
  switch ( input_mems[mem_index].getContentType() )
  {
    case ContentType::Blob :
     uint32_t res = *( T* )RuntimeStorage::getInstance().getBlob( input_mems[mem_index] )[ ofst ];
     return res;

    default :
     throw runtime_error ( "No write access." );
  }
}

void Invocation::attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type )
{
  if ( output_index >= output_count_ )
  {
    output_count_ = output_index + 1; 
  }

  outputs.emplace_back( output_index, output_type );
  output_mems[ mem_index ] = &outputs.back(); 

  return;
}

void Invocation::attach_output_child( uint32_t parent_mem_index, uint32_t child_index, uint32_t child_mem_index, uint32_t output_type )
{ 
  OutputTemp * parent_mem = output_mems[parent_mem_index];
  
  switch ( parent_mem->content_type_ )
  {
    case BLOB :
    case THUNK :
      throw runtime_error ( "Cannot add child to non-Tree output." );

    default :
      break;
  }
  
  if ( child_index >= get<InProgressTree>( parent_mem->content_ ) )
  {
    parent_mem->content_.emplace<InProgressTree>( child_index + 1 );
  }

  outputs.emplace_back( parent_mem->path_, child_index, output_type );
  output_mems[ child_mem_index ] = &outputs.back();

  return;
}

template<typename T>
void Invocation::store_i32( uint32_t mem_index, uint32_t content )
{
  OutputTemp * output_mem = output_mems[mem_index];
  switch ( output_mem->content_type_ )
  {
    case BLOB :
     T content_T = ( T )content; 
     get<InProgressBlob>( output_mem->content_ ).append( (char *)&content_T, sizeof( T )  );
     return;

    default:
     throw runtime_error ( "Cannot write to non-Blob output." ); 
  }
}

void Invocation::set_encode( uint32_t mem_index, uint32_t encode_mem_index )
{
  OutputTemp * output_mem = output_mems[mem_index];
  
  switch ( output_mem->content_type_ )
  { 
    case THUNK :
      break;

    default :
      throw runtime_error ( "Cannot set encode to a non-Thunk output." );
  }

  OutputTemp * encode_mem = output_mems[encode_mem_index];
  get<InProgressThunk>( output_mem->content_ ).encode_path_ = encode_mem->path_;

  return;
}

void Invocation::add_path( uint32_t mem_index, uint32_t path_index )
{
  OutputTemp * output_mem = output_mems[mem_index];
  
  switch ( output_mem->content_type_ )
  {
    case THUNK :
      break;

    default :
      throw runtime_error ( "Cannot set encode to a non-Thunk output." );
  }

  get<InProgressThunk>( output_mem->content_ ).path_.push_back( path_index );
  return;
}
  

      
