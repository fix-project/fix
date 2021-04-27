#include <cassert> 

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"

using namespace std;

string_view RuntimeStorage::getBlob( const Name & name )
{
  switch ( name.getType() )
  {
    case NameType::Literal :
      return name.getContent();

    case NameType::Canonical :
      return name_to_blob_.get( name ).content();

    case NameType::Thunk :
      return name_to_blob_.get( name ).content();
    
    default:
      throw out_of_range( "Blob does not exist." );
  }
}

const Tree & RuntimeStorage::getTree( const Name & name )
{
  switch ( name.getType() )
  {
    case NameType::Literal :
      return reinterpret_cast<const Tree &>( name.getContent() );

    case NameType::Canonical :
      return name_to_tree_.get( name );

    case NameType::Thunk :
      return name_to_tree_.get( name );

    default:
      throw out_of_range( "Tree does not exist." );
  }
}

void RuntimeStorage::addWasm( const string & name, const string & wasm_content, const string & wasm_rt_content )
{
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( name, wasm_content );

  string elf_content = c_to_elf( name, c_header, h_header, wasm_rt_content );

  addProgram( name, vector<string>(), vector<string>(), elf_content );
}
  
void RuntimeStorage::addProgram( const string& name, vector<string>&& inputs, vector<string>&& outputs, string & program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.insert_or_assign( name, link_program( elf_info, name, move( inputs ), move( outputs ) ) );
}

void RuntimeStorage::force( const Name & name )
{
  switch( name.getContentType() )
  {
    case ContentType::Blob :
     return;

    case ContentType::Tree : 
     this->forceTree( name );
     return;

    case ContentType::Thunk : 
     switch( name.getType() )
     {
       case NameType::Literal :
         this->forceThunk( reinterpret_cast<const Thunk &>( name.getContent() ) );
         return;
         
       case NameType::Canonical : 
         this->forceThunk( name_to_thunk_.get( name ) );
         return;

       default:
         return;
     } 

    default:
     return;
  }
}

void RuntimeStorage::forceTree( const Name & tree_name )
{
  const Tree & tree = this->getTree( tree_name );
  for ( const auto & name : tree )
  {
    this->force( name );
  }
}

void RuntimeStorage::forceThunk( const Thunk & thunk )
{
  this->evaluateEncode( thunk.getEncode() );   
}

void RuntimeStorage::prepareEncode( const Tree & encode, Invocation & invocation )
{
  this->forceTree( encode.at( 1 ) );
  Name function_name = encode.at( 0 );

  switch ( function_name.getContentType() ) 
  {
    case ContentType::Blob :
      invocation.setProgramName( function_name.getContent() );
      return;

    case ContentType::Tree :
      this->prepareEncode( this->getTree( function_name ), invocation );
      return;

    default :
      throw runtime_error ( "Invalid encode!" );
  }
}

void RuntimeStorage::evaluateEncode( const Name & encode_name )
{
  if ( this->getTree( encode_name ).size() != 3 )
  { 
    throw runtime_error ( "Invalid encode!" );
  }

  uint64_t curr_inv_id = 0; 
  curr_inv_id = Invocation::next_invocation_id_;
  Invocation::next_invocation_id_++;
  Invocation invocation ( encode_name );

  this->prepareEncode( this->getTree( encode_name ), invocation );

  const auto & program = name_to_program_.at( invocation.getProgramName() );
  invocation.setMem( reinterpret_cast<wasm_rt_memory_t *>( program.getMemLoc() ) );

  wasi::id_to_inv_.try_emplace( curr_inv_id, invocation );
  wasi::invocation_id_ = curr_inv_id;
  wasi::buf.size = 0;

  program.execute();

  wasi::id_to_inv_.erase( curr_inv_id );
}
