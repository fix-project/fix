#include "runtimes.hh"
#include "overload.hh"
#include "storage_exception.hh"

using namespace std;

shared_ptr<ReadOnlyRT> ReadOnlyRT::init()
{
  auto runtime = std::make_shared<ReadOnlyRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
  return runtime;
}

shared_ptr<ReadWriteRT> ReadWriteRT::init()
{
  auto runtime = std::make_shared<ReadWriteRT>();
  runtime->repository_.emplace();
  runtime->executor_.emplace( std::thread::hardware_concurrency(), runtime );
  return runtime;
}

optional<BlobData> ReadOnlyRT::get( Handle<Named> name )
{
  if ( executor_->contains( name ) ) {
    return executor_->get( name );
  } else if ( repository_->contains( name ) ) {
    auto blob = repository_->get( name );
    executor_->put( name, *blob );
    return *blob;
  }

  throw HandleNotFound( name );
}

optional<TreeData> ReadOnlyRT::get( Handle<AnyTree> name )
{
  if ( executor_->contains( name ) ) {
    return executor_->get( name );
  } else if ( repository_->contains( name ) ) {
    auto tree = repository_->get( name );
    executor_->put( name, *tree );
    return *tree;
  }

  throw HandleNotFound( handle::fix( name ) );
};

optional<Handle<Object>> ReadOnlyRT::get( Handle<Relation> name )
{
  if ( repository_->contains( name ) ) {
    auto relation = repository_->get( name );
    repository_->put( name, *relation );
    return *relation;
  } else {
    return executor_->get( name );
  }
};

void ReadOnlyRT::put( Handle<Named> name, BlobData data )
{
  executor_->put( name, data );
}

void ReadOnlyRT::put( Handle<AnyTree> name, TreeData data )
{
  executor_->put( name, data );
}

void ReadOnlyRT::put( Handle<Relation> name, Handle<Object> data )
{
  executor_->put( name, data );
}

bool ReadOnlyRT::contains( Handle<Named> handle )
{
  return executor_->contains( handle ) || repository_->contains( handle );
}

bool ReadOnlyRT::contains( Handle<AnyTree> handle )
{
  return executor_->contains( handle ) || repository_->contains( handle );
}

bool ReadOnlyRT::contains( Handle<Relation> handle )
{
  return executor_->contains( handle ) || repository_->contains( handle );
}

bool ReadOnlyRT::contains( const std::string_view label )
{
  return repository_->contains( label ) || executor_->contains( label );
}

Handle<Fix> ReadOnlyRT::labeled( const std::string_view label )
{
  if ( repository_->contains( label ) ) {
    return repository_->labeled( label );
  }

  return executor_->labeled( label );
}

Handle<Value> ReadOnlyRT::execute( Handle<Relation> x )
{
  return executor_->execute( x );
}

template<FixType T>
void ReadWriteRT::serialize( Handle<T> handle )
{
  if constexpr ( Handle<T>::is_fix_sum_type and not Handle<T>::is_fix_wrapper ) {
    return std::visit( [this]( const auto x ) { return this->serialize( x ); }, handle.get() );
  } else {
    if constexpr ( is_same_v<T, Named> or FixTreeType<T> or is_same_v<T, Apply> or is_same_v<T, Eval> ) {
      repository_->put( handle, executor_->get( handle ).value() );
    }
  }
}

Handle<Value> ReadWriteRT::execute( Handle<Relation> x )
{
  auto res = executor_->execute( x );

  cout << "Serializing minrepo" << endl;
  executor_->visit( x, [this]( Handle<Fix> h ) { serialize( h ); } );
  return res;
}
