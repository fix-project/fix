#pragma once
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

#include "handle.hh"
#include "interface.hh"
#include "object.hh"

class Repository : public IRuntime
{
  std::filesystem::path repo_;

public:
  Repository( std::filesystem::path directory = std::filesystem::current_path() );
  static std::filesystem::path find( std::filesystem::path directory = std::filesystem::current_path() );

  std::unordered_set<Handle<Fix>> data() const;
  std::unordered_set<std::string> labels() const;
  std::unordered_map<Handle<Fix>, std::unordered_set<Handle<Fix>>> pins() const;

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> relation ) override;

  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;

  Handle<Fix> labeled( const std::string_view label ) override;
  void label( const std::string_view label, Handle<Fix> target );

  std::unordered_set<Handle<Fix>> pinned( Handle<Fix> src ) const;
  void pin( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts );

  virtual bool contains( Handle<Named> name ) override;
  virtual bool contains( Handle<AnyTree> name ) override;
  virtual bool contains( Handle<Relation> name ) override;
  virtual std::optional<Handle<AnyTree>> contains( Handle<AnyTreeRef> name ) override;

  bool contains( const std::string_view label ) override;

  std::filesystem::path path() { return repo_; }

  Handle<Fix> lookup( const std::string_view ref );
};
