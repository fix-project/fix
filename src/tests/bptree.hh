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
