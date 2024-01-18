#pragma once

#include "handle.hh"
#include "handle_util.hh"
#include "object.hh"
#include "overload.hh"

/**
 * A basic Runtime environment, capable of loading/storing Fix data and (if parallelism != 0) discovering new Fix
 * relations.
 */
class IRuntime
{
public:
  /**
   * Metadata describing this Runtime.
   */
  struct Info
  {
    uint32_t parallelism;
    double link_speed;
  };

  /*
   * Assigns the Handle @p name to this IRuntime.  If the IRuntime already has the relevant data, it may return it
   * immediately.  If it returns `std::nullopt`, it guarantees that it will eventually learn the result (either by
   * computing it or by delegating to another runtime).
   *
   * Runtimes which have no ability to get this result should throw a StorageException.
   *
   * @param name  The name of the data to load.
   * @return      The corresponding data.
   */
  ///@{
  virtual std::optional<BlobData> get( Handle<Named> name ) = 0;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) = 0;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) = 0;
  ///}@

  /**
   * Notifies this IRuntime that the data corresponding to @p name is @p data.  Each function has two variants, one
   * which provides a reference and once which provides move semantics.
   *
   * @param name  The name corresponding to the data.
   * @param data  A reference-counted pointer to the data.
   */
  ///@{
  virtual void put( Handle<Named> name, BlobData data ) = 0;
  virtual void put( Handle<AnyTree> name, TreeData data ) = 0;
  virtual void put( Handle<Relation> name, Handle<Object> data ) = 0;
  ///@}

  /**
   * These functions automatically compute the canonical name of data before storing them.  Implementors may
   * choose to override them to create local names; however, they should ensure there will be no confusion between
   * local names generated independently.  This probably means names should be canonicalized before leaving a
   * IRuntime which maintains local names.
   *
   * @param data  A reference-counted pointer to the data.
   * @return      The Handle of the data.
   */
  ///@{
  virtual Handle<Blob> create( BlobData data )
  {
    auto handle = handle::create( data );
    handle.visit<void>( overload {
      [&]( Handle<Named> name ) { put( name, data ); },
      []( Handle<Literal> ) {},
    } );
    return handle;
  }

  virtual Handle<AnyTree> create( TreeData data )
  {
    auto handle = handle::create( data );
    put( handle, data );
    return handle;
  }
  ///@}

  /**
   * Tests if this IRuntime contains the given data.  This response must not have any false positives, but may have
   * false negatives.

   * @param handle  The handle to query.
   * @return        Whether the data is present in this IRuntime.
   */
  virtual bool contains( Handle<Fix> handle ) = 0;

  /**
   * Gets metadata about this IDataLoader.
   *
   * @return The loader's information, if known.
   */
  virtual std::optional<Info> get_info() { return {}; }

  virtual ~IRuntime() {}
};
