#include <cassert> 
#include <iostream>

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"
#include "wasm-rt-content.h"

using namespace std;

string_view RuntimeStorage::getBlob( const Name & name )
{ 
  if ( name_to_thunk_.contains( name ) )
  {
    Name temp_name = name;

    while ( name_to_thunk_.contains( temp_name ) ) 
    {
      const auto & thunk = name_to_thunk_.get( temp_name );
      temp_name = Name( thunk.getEncode(), thunk.getPath(), ContentType::Unknown );
    }

    return name_to_blob_.get( temp_name ).content();
  }

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
      return name.getTreeContent(); 

    case NameType::Canonical :
    case NameType::Thunk :
      return name_to_tree_.get( name );

    default:
      throw out_of_range( "Tree does not exist." );
  }
}

void RuntimeStorage::addWasm( const string & name, const string & wasm_content, const vector<string> & deps )
{
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( name, wasm_content );

  string elf_content = c_to_elf( name, c_header, h_header, wasm_rt_content );

  addProgram( name, vector<string>(), vector<string>(), elf_content );
  name_to_program_.at( name ).setDeps( deps );
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

       case NameType::Thunk :
         this->forceThunk( name_to_thunk_.get( name ) );
         return;

       default:
         return;
     } 

    case ContentType::Unknown :
     if ( name_to_tree_.contains( name ) )
     {
       this->forceTree( name_to_tree_.get( name ) );
     } else if ( name_to_blob_.contains( name ) )
     {
     } else if ( name_to_thunk_.contains( name ) )
     {
       this->forceThunk( name_to_thunk_.get( name ) );
     }
     return;

    default:
     return;
  }
}

void RuntimeStorage::forceTree( const Name & tree_name )
{
  for ( const auto & name : this->getTree( tree_name ) )
  {
    this->force( name );
  }
}

void RuntimeStorage::forceThunk( const Thunk & thunk )
{
  vector<size_t> path = { 0 };
  Name res_name ( thunk.getEncode(), path, ContentType::Thunk );
  
  this->evaluateEncode( thunk.getEncode() );  

  if ( name_to_thunk_.contains( res_name ) )
  { 
    const Thunk & sub_thunk = name_to_thunk_.get( res_name );
    this->forceThunk( sub_thunk );
  } else if ( !name_to_blob_.contains( res_name ) )
  {
    if ( name_to_tree_.contains( res_name ) )
    {
      cout << "A tree" << endl;
    }
    if ( thunk_to_blob_.contains( res_name ) )
    {
      cout << "A name" << endl;
    }

  }
}

void RuntimeStorage::prepareEncode( const Tree & encode, Invocation & invocation )
{
  Name function_name = encode.at( 0 );
  this->forceTree( encode.at( 1 ) );
  if ( function_name.getType() == NameType::Thunk )
  {
    function_name = thunk_to_blob_.at( function_name );
  }

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
  if ( this->getTree( encode_name ).size() > 3 )
  { 
    throw runtime_error ( "Invalid encode!" );
  }

  uint64_t curr_inv_id = 0; 
  curr_inv_id = Invocation::next_invocation_id_;
  Invocation::next_invocation_id_++;
  Invocation invocation ( encode_name );

  // Name temp_name = encode_name;

  // cout << "here " << encode_name << endl;
  const Tree & encode = this->getTree( encode_name );
  // cout << "Encode of size " << encode.size() << endl;
  this->prepareEncode( encode, invocation );
  // cout << "hhere " << encode_name << endl;

  const auto & program = name_to_program_.at( invocation.getProgramName() );
  invocation.setMem( reinterpret_cast<wasm_rt_memory_t *>( program.getMemLoc() ) );

  wasi::id_to_inv_.try_emplace( curr_inv_id, invocation );
  wasi::invocation_id_ = curr_inv_id;
  wasi::buf.size = 0;
 
  //cout << "Executing " << invocation.getProgramName() << " on " << endl;
  //cout << encode_name << endl;
  //cout << temp_name << endl;
  //const auto & temp_encode = this->getTree( encode_name );
  //const Name & strict_input = temp_encode.at( 1 ); 
  //for ( const auto & name : this->getTree( strict_input ) )
  //{
   // cout << name;
  //}
  program.execute();

  wasi::id_to_inv_.at( curr_inv_id ).add_to_storage();

  wasi::id_to_inv_.erase( curr_inv_id );
}

Name RuntimeStorage::addBlob( string && blob_content )
{
  if ( blob_content.length() > 32 )
  {
    throw runtime_error ( "Not implemented yet." );
  }

  Name name ( move( blob_content ), NameType::Literal, ContentType::Blob );

  return name;
}

Name RuntimeStorage::addTree( vector<Name> && tree_content )
{
  if ( tree_content.size() > 32 )
  {
    throw runtime_error ( "Not implemetned yet." );
  }

  Name name ( tree_content );

  return name;
}

Name RuntimeStorage::addEncode( const Name & program_name, const Name & strict_input, const Name & lazy_input )
{
  Tree encode;
  
  encode.push_back( program_name );
  encode.push_back( strict_input );
  encode.push_back( lazy_input );

  return this->addTree( move( encode ) );
}  


