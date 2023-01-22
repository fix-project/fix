template<typename T>
class concurrent_vector
{
private:
  std::vector<T> vector_;
  std::mutex vector_mutex_;

public:
  concurrent_vector()
    : vector_()
    , vector_mutex_()
  {}

  T& at( const std::vector<T>::size_type pos )
  {
    std::lock_guard<std::mutex> lock( vector_mutex_ );
    return vector_.at( pos );
  }

  std::vector<T>::size_type size()
  {
    std::lock_guard<std::mutex> lock( vector_mutex_ );
    return vector_.size();
  }

  void push_back( const T& value )
  {
    std::lock_guard<std::mutex> lock( vector_mutex_ );
    vector_.push_back( std::move( value ) );
  }

  void push_back( T&& value )
  {
    std::lock_guard<std::mutex> lock( vector_mutex_ );
    vector_.push_back( std::move( value ) );
  }
};
