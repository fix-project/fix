#pragma once
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include "handle.hh"
#include "object.hh"

class Repository
{
  std::filesystem::path repo_;

public:
  Repository( std::filesystem::path directory = std::filesystem::current_path() );

  std::unordered_set<Handle<Fix>> data() const;
  std::unordered_set<std::string> labels() const;
  std::unordered_map<Handle<Fix>, std::unordered_set<Handle<Fix>>> pins() const;

  OwnedBlob get( Handle<Named> name ) const;
  template<FixTreeType T>
  OwnedTree get( Handle<T> name ) const;
  Handle<Fix> get( Handle<Relation> relation ) const;

  void put( Handle<Named> name, OwnedBlob& data );
  template<FixTreeType T>
  void put( Handle<T> name, OwnedTree& data );
  void put( Handle<Relation> relation, Handle<Fix> target );

  Handle<Fix> labeled( const std::string_view label ) const;
  void label( const std::string_view label, Handle<Fix> target );

  std::unordered_set<Handle<Fix>> pinned( Handle<Fix> src ) const;
  void pin( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts );

  bool contains( Handle<Fix> handle ) const;
  bool contains( const std::string_view label ) const;
};
