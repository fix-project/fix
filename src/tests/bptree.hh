#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

template<typename Key, typename Value>
class Node;

template<typename Key, typename Value>
class BPTree;

template<typename Key, typename Value>
class Node
{
  std::vector<Key> keys_ {};
  std::vector<std::shared_ptr<Node>> children_ {};
  std::vector<Value> data_ {};
  bool isleaf_ { true };

  friend class BPTree<Key, Value>;

  std::pair<std::shared_ptr<Node>, Key> split();
  void dfs_visit( std::function<void( Node* )> visitor );

  std::string key_data_buf_ {};

public:
  Node() {}
  const std::vector<Key>& get_keys() const { return keys_; }
  std::string_view get_key_data()
  {
    return { reinterpret_cast<const char*>( keys_.data() ), keys_.size() * sizeof( Key ) };
  }
  const std::vector<Value>& get_data() const { return data_; }
  const std::vector<std::shared_ptr<Node>> get_children() const { return children_; }
  bool is_leaf() const { return isleaf_; }

  void set_keys( std::vector<Key> keys ) { keys_ = keys; }
  void set_data( std::vector<Value> data )
  {
    if ( !isleaf_ ) {
      throw std::runtime_error( "Non leaf BPTree node should not have data." );
    }
    data_ = data;
  }
};

template<typename Key, typename Value>
class BPTree
{
  const size_t degree_;
  std::shared_ptr<Node<Key, Value>> root_ { std::make_shared<Node<Key, Value>>() };

public:
  BPTree( size_t degree )
    : degree_( degree )
  {}

  void insert( Key key, Value value );
  void dfs_visit( std::function<void( Node<Key, Value>* )> visitor );
  std::optional<Value> get( Key key );
  std::shared_ptr<Node<Key, Value>> get_root() { return root_; }

  void set_root( std::shared_ptr<Node<Key, Value>> root ) { root_ = root; }

  size_t get_degree() const { return degree_; }
};

template<typename Key, typename Value>
std::pair<std::shared_ptr<Node<Key, Value>>, Key> Node<Key, Value>::split()
{
  auto res = std::make_shared<Node>();

  size_t cutoff = keys_.size() / 2;
  auto middle_key = keys_[cutoff];

  res->isleaf_ = isleaf_;

  res->keys_ = { keys_.begin(), keys_.begin() + cutoff };
  if ( res->isleaf_ ) {
    keys_.erase( keys_.begin(), keys_.begin() + cutoff );
  } else {
    // Erase the middle key
    keys_.erase( keys_.begin(), keys_.begin() + cutoff + 1 );
  }

  if ( res->isleaf_ ) {
    res->data_ = { data_.begin(), data_.begin() + cutoff };
    data_.erase( data_.begin(), data_.begin() + cutoff );
  } else {
    res->children_ = { children_.begin(), children_.begin() + cutoff + 1 };
    children_.erase( children_.begin(), children_.begin() + cutoff + 1 );
  }

  return { res, middle_key };
}

template<typename Key, typename Value>
void BPTree<Key, Value>::insert( Key key, Value value )
{
  std::shared_ptr<Node<Key, Value>> cursor = root_;
  std::deque<std::shared_ptr<Node<Key, Value>>> path;

  while ( cursor->isleaf_ == false ) {
    path.push_back( cursor );
    size_t idx = upper_bound( cursor->keys_.begin(), cursor->keys_.end(), key ) - cursor->keys_.begin();
    cursor = cursor->children_[idx];
  }

  auto pos = upper_bound( cursor->keys_.begin(), cursor->keys_.end(), key );
  auto idx = pos - cursor->keys_.begin();
  if ( pos != cursor->keys_.begin() && *( pos - 1 ) == key )
    return;
  cursor->keys_.insert( pos, key );
  cursor->data_.insert( cursor->data_.begin() + idx, value );
  if ( cursor->keys_.size() <= degree_ )
    return;

  auto new_node = cursor->split();

  // Traverse back the path
  while ( path.size() > 0 ) {
    cursor = path.back();
    path.pop_back();

    auto pos = upper_bound( cursor->keys_.begin(), cursor->keys_.end(), new_node.second );
    auto idx = pos - cursor->keys_.begin();

    cursor->keys_.insert( pos, new_node.second );
    cursor->children_.insert( cursor->children_.begin() + idx, new_node.first );
    if ( cursor->children_.size() <= degree_ )
      return;

    new_node = cursor->split();
  }

  // Create a new root
  auto new_root = std::make_shared<Node<Key, Value>>();
  new_root->isleaf_ = false;
  new_root->keys_.push_back( new_node.second );
  new_root->children_.push_back( new_node.first );
  new_root->children_.push_back( root_ );
  root_ = new_root;

  return;
}

template<typename Key, typename Value>
std::optional<Value> BPTree<Key, Value>::get( Key key )
{
  std::shared_ptr<Node<Key, Value>> cursor = root_;

  while ( cursor->isleaf_ == false ) {
    size_t idx = upper_bound( cursor->keys_.begin(), cursor->keys_.end(), key ) - cursor->keys_.begin();
    cursor = cursor->children_[idx];
  }

  auto pos = upper_bound( cursor->keys_.begin(), cursor->keys_.end(), key );
  auto idx = pos - cursor->keys_.begin() - 1;
  if ( pos != cursor->keys_.begin() && *( pos - 1 ) == key ) {
    return cursor->data_[idx];
  } else {
    return {};
  }
}

template<typename Key, typename Value>
void Node<Key, Value>::dfs_visit( std::function<void( Node<Key, Value>* )> visitor )
{
  for ( const auto& child : children_ ) {
    child->dfs_visit( visitor );
  }
  visitor( this );
}

template<typename Key, typename Value>
void BPTree<Key, Value>::dfs_visit( std::function<void( Node<Key, Value>* )> visitor )
{
  root_->dfs_visit( visitor );
}
