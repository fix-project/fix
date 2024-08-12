#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

class Node;
class BPTree;

class Node
{
  std::vector<size_t> keys_ {};
  std::vector<std::shared_ptr<Node>> children_ {};
  std::vector<std::string> data_ {};
  bool isleaf_ { true };

  friend class BPTree;

  std::pair<std::shared_ptr<Node>, size_t> split();
  void dfs_visit( std::function<void( Node* )> visitor );

public:
  Node() {}
  const std::vector<size_t>& get_keys() const { return keys_; }
  const std::vector<std::string>& get_data() const { return data_; }
  const std::vector<std::shared_ptr<Node>> get_children() const { return children_; }
  bool is_leaf() const { return isleaf_; }
};

class BPTree
{
  const size_t degree_;
  std::shared_ptr<Node> root_ { std::make_shared<Node>() };

public:
  BPTree( size_t degree )
    : degree_( degree )
  {}

  void insert( size_t key, std::string value );
  void dfs_visit( std::function<void( Node* )> visitor );
  std::optional<std::string> get( size_t key );
  std::shared_ptr<Node> get_root() { return root_; }
};

std::pair<std::shared_ptr<Node>, size_t> Node::split()
{
  auto res = std::make_shared<Node>();

  size_t cutoff = keys_.size() / 2;
  size_t middle_key = keys_[cutoff];

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

void BPTree::insert( size_t key, std::string value )
{
  std::shared_ptr<Node> cursor = root_;
  std::deque<std::shared_ptr<Node>> path;

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
  auto new_root = std::make_shared<Node>();
  new_root->isleaf_ = false;
  new_root->keys_.push_back( new_node.second );
  new_root->children_.push_back( new_node.first );
  new_root->children_.push_back( root_ );
  root_ = new_root;

  return;
}

std::optional<std::string> BPTree::get( size_t key )
{
  std::shared_ptr<Node> cursor = root_;

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

void Node::dfs_visit( std::function<void( Node* )> visitor )
{
  for ( const auto& child : children_ ) {
    child->dfs_visit( visitor );
  }
  visitor( this );
}

void BPTree::dfs_visit( std::function<void( Node* )> visitor )
{
  root_->dfs_visit( visitor );
}
