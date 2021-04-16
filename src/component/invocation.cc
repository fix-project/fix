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

  input_mems.insert_or_assign( mem_index, InputMem( input_name ) );
}

template<typename T>
uint32_t Invocation::get_i32( uint32_t mem_index, uint32_t ofst )
{
  switch ( input_mems.at( mem_index ).name_.getContentType() )
  {
    case ContentType::Blob :
     uint32_t res = *( T* )RuntimeStorage::getInstance().getBlob( input_mems.at( mem_index ).name_ )[ ofst ];
     return res;

    default :
     throw runtime_error ( "No write access." );
  }
}

void Invocation::attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type )
{
  if ( output_index >= output_tree_.childs.size() )
  {
    output_tree_.childs.resize( output_index + 1 );
  }

  output_tree_.childs.emplace( output_tree_.childs.begin() + ouptut_index, output_type );
  output_mems.insert_or_assign( mem_index, &output_tree_.childs.at( output_index ) );

  return;
}

void Invocation::attach_output_child( uint32_t parent_mem_index, uint32_t child_index, uint32_t child_mem_index, uint32_t output_type )
{ 
  OutputTemp * parent_mem = output_mems.at( parent_mem_index );
  
  switch ( parent_mem->name_.getContentType() )
  {
    case ContentType::Blob :
    case ContentType::Thunk :
      throw runtime_error ( "Cannot add child to non-Tree output." );

    default :
      continue;
  }
  
  if ( child_index >= parent_mem->childs.size() )
  {
    parent_mem->childs.resize( child_index + 1 );
  }

  parent_mem->childs.emplace( parent_mem->childs.begin + child_index, output_type );
  output_mems.insert_or_assign( child_mem_index, parent_mem->childs.at( child_index ));

  return;
}

template<typename T>
uint32_t Invocation::store_i32( uint32_t mem_index, uint32_t content )
{
  OutputTemp * output_mem = output_mems.at( mem_index );
  switch ( output_mem->name_.getContentType() )
  {
    case ContentType::Blob :
     T content_T = ( T )content; 
     ouptut_mem.buffer.append( (char *)&content_T, sizeof( T )  );
     return;

    default:
     throw runtime_error ( "Cannot write to non-Blob output." ); 
  }
}

void Invocation::set_encode( uint32_t mem_index, uint32_t encode_mem_index )
{
  OutputTemp * output_mem = output_mems.at( mem_index );
  
  switch ( output_mem->name_.getContentType() )
  {
    case ContentType::Blob :
    case ContentType::Tree :
      throw runtime_error ( "Cannot set encode to a non-Thunk output." );
      
    default :
      continue;
  }

  OutputTemp * encode_mem = output_mems.at( encode_mem_index );

  switch ( encode_mem_index->name_.getContentType() )
  {
    case ContentType::Tree :
      output_mem->encode = encode_mem;
      return;

    default :
      throw runtime_error ( "Cannot set Blob or Thunk output as encode." );
  }
}

void Invocation::add_path( uint32_t mem_index, uint32_t path )
{
  OutputTemp * output_mem = output_mems.at( mem_index );
  
  switch ( output_mem->name_.getContentType() )
  {
    case ContentType::Blob :
    case ContentType::Tree :
      throw runtime_error ( "Cannot set encode to a non-Thunk output." );
      
    default :
      continue;
  }

  output_mem->path.push_back( static_cast<int>( path ) );
  return;
}
  

      
