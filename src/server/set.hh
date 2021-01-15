#pragma once

#include <array>
#include <iterator>
#include <optional>
#include <queue>

template<typename T>
class Set
{
  std::vector<std::optional<T>> elems_ {};
  std::queue<size_t> available_ids_ {};

  template<class underlying_type, class reference>
  class iteratorT
  {
    underlying_type it_;
    underlying_type end_;

    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;

  public:
    iteratorT( const underlying_type it, const underlying_type end_it )
      : it_( it )
      , end_( end_it )
    {}

    reference operator*() { return it_->value(); }
    iteratorT& operator++()
    {
      do {
        it_++;
      } while ( ( it_ != end_ ) and ( not it_->has_value() ) );
      return *this;
    }

    void find_first()
    {
      while ( ( it_ != end_ ) and ( not it_->has_value() ) ) {
        it_++;
      }
    }

    friend bool operator==( const iteratorT& a, const iteratorT& b ) { return a.it_ == b.it_; }
    friend bool operator!=( const iteratorT& a, const iteratorT& b ) { return a.it_ != b.it_; }
  };

  using iterator = iteratorT<typename std::vector<std::optional<T>>::iterator, T&>;
  using const_iterator = iteratorT<typename std::vector<std::optional<T>>::const_iterator, const T&>;

public:
  size_t get_index()
  {
    if ( available_ids_.empty() ) {
      elems_.emplace_back();
      available_ids_.push( elems_.size() - 1 );
    }

    const size_t ret = available_ids_.front();
    return ret;
  }

  void insert( const size_t index, T&& elem )
  {
    if ( available_ids_.empty() or index != available_ids_.front() ) {
      throw std::runtime_error( "Set: index " + std::to_string( index ) + " not available" );
    }

    if ( has_value( index ) ) {
      throw std::runtime_error( "Set internal error: index " + std::to_string( index ) + " not available" );
    }

    elems_.at( index ).emplace( std::move( elem ) );
    available_ids_.pop();
  }

  void erase( const size_t index )
  {
    elems_.at( index ).reset();
    available_ids_.push( index );
  }

  bool has_value( const size_t index ) const { return index < elems_.size() and elems_.at( index ).has_value(); }
  size_t capacity() const { return elems_.size(); }

  const T& at( const size_t index ) const
  {
    if ( not elems_.at( index ).has_value() ) {
      throw std::out_of_range( "Set: " + std::to_string( index ) );
    }
    return *elems_.at( index );
  }

  T& at( const size_t index )
  {
    if ( not elems_.at( index ).has_value() ) {
      throw std::out_of_range( "Set: " + std::to_string( index ) );
    }
    return *elems_.at( index );
  }

  iterator begin()
  {
    iterator ret { elems_.begin(), elems_.end() };
    ret.find_first();
    return ret;
  }

  iterator end() { return { elems_.end(), elems_.end() }; }

  const_iterator begin() const
  {
    const_iterator ret { elems_.begin(), elems_.end() };
    ret.find_first();
    return ret;
  }

  const_iterator end() const { return { elems_.end(), elems_.end() }; }
};
