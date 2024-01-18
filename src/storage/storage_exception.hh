#pragma once
#include <filesystem>
#include <format>

#include "base16.hh"
#include "handle.hh"

class StorageException : public std::exception
{
  std::string message_;

public:
  StorageException( std::string message )
    : message_( message )
  {}
  virtual const char* what() const noexcept override { return message_.c_str(); }
};

class RepositoryNotFound : public StorageException
{
public:
  RepositoryNotFound( std::filesystem::path path )
    : StorageException(
      std::format( "Could not find fix repository in {} or any parent directory.", std::string( path ) ) )
  {}
};

class RepositoryCorrupt : public StorageException
{
public:
  RepositoryCorrupt( std::filesystem::path path )
    : StorageException( std::format( "Repository \"{}\" was corrupt.", std::string( path ) ) )
  {}
};

class HandleNotFound : public StorageException
{
public:
  HandleNotFound( Handle<Fix> handle )
    : StorageException( std::format( "Could not find handle {}.", base16::encode( handle.content ) ) )
  {}
};

class LabelNotFound : public StorageException
{
public:
  LabelNotFound( const std::string_view label )
    : StorageException( std::format( "Could not find label \"{}\".", label ) )
  {}
};

class ReferenceNotFound : public StorageException
{
public:
  ReferenceNotFound( const std::string_view ref )
    : StorageException( std::format( "No data matched reference \"{}\".", ref ) )
  {}
};

class AmbiguousReference : public StorageException
{
public:
  AmbiguousReference( const std::string_view ref )
    : StorageException( std::format( "Reference \"{}\" was ambiguous.", ref ) )
  {}
};
