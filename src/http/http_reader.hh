#pragma once

#include <cctype>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "http_structures.hh"

#define AUTO_READ                                                                                                  \
  size_t read( const std::string_view input )                                                                      \
  {                                                                                                                \
    read_all( input );                                                                                             \
    return input.size();                                                                                           \
  }

class StringReader
{
  std::string target_;

public:
  using Contained = std::string;
  Contained release() { return std::move( target_ ); }

  StringReader( Contained&& target )
    : target_( std::move( target ) )
  {
    target_.erase();
  }

  void read_all( const std::string_view input ) { target_.append( input ); }

  AUTO_READ

  ~StringReader()
  {
    if ( not target_.empty() ) {
      std::cerr << "Warning, destroying StringReader with contents.\n";
    }
  }
};

class LengthReader
{
  std::string target_;
  size_t target_length_;

public:
  using Contained = std::string;
  Contained release() { return std::move( target_ ); }

  LengthReader( Contained&& target, const size_t target_length )
    : target_( std::move( target ) )
    , target_length_( target_length )
  {
    target_.erase();
  }

  bool finished() const { return target_.size() == target_length_; }

  size_t read( const std::string_view input )
  {
    const std::string_view readable_portion = input.substr( 0, target_length_ - target_.size() );
    target_.append( readable_portion );
    return readable_portion.size();
  }

  ~LengthReader()
  {
    if ( not target_.empty() ) {
      std::cerr << "Warning, destroying LengthReader with contents.\n";
    }
  }
};

template<size_t TerminusLength, const char* TerminusData, class Inner>
class TerminusReader
{
  Inner target_;
  uint8_t chars_matched_;

  static constexpr std::string_view Terminus { TerminusData, TerminusLength };

public:
  using Contained = typename Inner::Contained;
  Contained release() { return target_.release(); }

  TerminusReader( Contained&& target )
    : target_( std::move( target ) )
    , chars_matched_()
  {}

  bool finished() const { return chars_matched_ == TerminusLength; }

  size_t read( const std::string_view original_input )
  {
    std::string_view input = original_input;

    while ( ( not finished() ) and ( not input.empty() ) ) {
      if ( chars_matched_ == 0 ) {
        /* Look for first character of terminus. */
        const size_t first_char_location = input.find( Terminus[0] );
        if ( first_char_location == std::string_view::npos ) {
          target_.read_all( input );
          input = {};
        } else {
          chars_matched_ = 1;
          target_.read_all( input.substr( 0, first_char_location ) );
          input.remove_prefix( first_char_location + 1 );
        }
      } else {
        /* Look for rest of terminus. */
        const size_t rest_of_match_length = std::min( input.size(), TerminusLength - chars_matched_ );
        const std::string_view match_needed = Terminus.substr( chars_matched_, rest_of_match_length );

        if ( input.substr( 0, rest_of_match_length ) == match_needed ) {
          chars_matched_ += rest_of_match_length;
          input.remove_prefix( rest_of_match_length );
        } else {
          target_.read_all( Terminus.substr( 0, chars_matched_ ) );
          chars_matched_ = 0;
        }
      }
    }

    return original_input.size() - input.size();
  }
};

namespace Termini {
inline constexpr char CRLF[] = "\r\n";
inline constexpr char COLON[] = ":";
inline constexpr char SPACE[] = " ";
}

template<class Inner>
using LineReader = TerminusReader<2, Termini::CRLF, Inner>;

template<class Inner>
using ColonReader = TerminusReader<1, Termini::COLON, Inner>;

template<class Inner>
using SpaceReader = TerminusReader<1, Termini::SPACE, Inner>;

template<class Inner>
class IgnoreInitialWhitespaceReader
{
  Inner target_;
  bool started_ = false;

public:
  using Contained = typename Inner::Contained;
  Contained release() { return target_.release(); }

  IgnoreInitialWhitespaceReader( Contained&& target )
    : target_( std::move( target ) )
  {}

  void read_all( std::string_view input )
  {
    if ( started_ ) {
      return target_.read_all( input );
    }

    const size_t non_ws_location = input.find_first_not_of( " \t" );
    if ( non_ws_location == std::string_view::npos ) {
      return;
    }

    // found non-whitespace
    started_ = true;
    input.remove_prefix( non_ws_location );
    target_.read_all( input );
  }

  AUTO_READ
};

template<class Inner>
class IgnoreTrailingWhitespaceReader
{
  Inner target_;

public:
  using Contained = typename Inner::Contained;
  Contained release()
  {
    Contained output = target_.release();

    const size_t non_ws_location = output.find_last_not_of( " \t" );
    if ( non_ws_location == std::string_view::npos ) {
      output.erase();
    } else {
      output.resize( non_ws_location + 1 );
    }

    return output;
  }

  IgnoreTrailingWhitespaceReader( Contained&& target )
    : target_( std::move( target ) )
  {}

  void read_all( const std::string_view input ) { target_.read_all( input ); }

  AUTO_READ
};

template<class Inner>
class IgnoreSurroundingWhitespaceReader
  : public IgnoreTrailingWhitespaceReader<IgnoreInitialWhitespaceReader<Inner>>
{
public:
  using IgnoreTrailingWhitespaceReader<IgnoreInitialWhitespaceReader<Inner>>::IgnoreTrailingWhitespaceReader;
};

template<class First, class Second>
class PairReader
{
  First first_;
  Second second_;

public:
  using Contained = std::pair<typename First::Contained, typename Second::Contained>;
  Contained release() { return { first_.release(), second_.release() }; }

  PairReader( Contained&& target )
    : first_( std::move( target.first ) )
    , second_( std::move( target.second ) )
  {}

  size_t read( const std::string_view original_input )
  {
    std::string_view input = original_input;
    input.remove_prefix( first_.read( input ) );
    input.remove_prefix( second_.read( input ) );
    return original_input.size() - input.size();
  }

  void read_all( std::string_view input )
  {
    input.remove_prefix( first_.read( input ) );
    second_.read_all( input );
  }
};

class HTTPHeaderReader
{
  HTTPHeaders target_ {};
  LineReader<PairReader<ColonReader<StringReader>, IgnoreSurroundingWhitespaceReader<StringReader>>> current_line_;
  bool finished_ {};

public:
  static bool header_equals( const std::string_view a, const std::string_view b )
  {
    return std::equal( a.begin(), a.end(), b.begin(), b.end(), []( const char a_ch, const char b_ch ) {
      return tolower( a_ch ) == tolower( b_ch );
    } );
  }

  HTTPHeaders release() { return std::move( target_ ); }

  using State = decltype( current_line_ )::Contained;
  HTTPHeaderReader( State&& state )
    : current_line_( std::move( state ) )
  {}

  bool finished() const { return finished_; }

  size_t read( const std::string_view orig_input )
  {
    std::string_view input = orig_input;
    while ( ( not finished() ) and ( not input.empty() ) ) {
      input.remove_prefix( current_line_.read( input ) );

      if ( current_line_.finished() ) {
        State state = current_line_.release();

        if ( state.first.empty() ) {
          finished_ = true;
        } else if ( header_equals( state.first, "Content-Length" ) ) {
          const auto num = stoi( state.second );
          if ( num < 0 ) {
            throw std::runtime_error( "invalid Content-Length" );
          }
          const size_t num_unsigned = num;
          if ( target_.content_length.has_value() ) {
            if ( target_.content_length.value() != num_unsigned ) {
              throw std::runtime_error( "contradictory Content-Length headers" );
            }
          } else {
            target_.content_length.emplace( num_unsigned );
          }
        } else if ( header_equals( state.first, "Host" ) ) {
          target_.host = state.second;
        } else if ( header_equals( state.first, "Connection" ) ) {
          target_.connection = state.second;
        } else if ( header_equals( state.first, "Upgrade" ) ) {
          target_.upgrade = state.second;
        } else if ( header_equals( state.first, "Origin" ) ) {
          target_.origin = state.second;
        } else if ( header_equals( state.first, "Sec-WebSocket-Key" ) ) {
          target_.sec_websocket_key = state.second;
        } else if ( header_equals( state.first, "Sec-WebSocket-Accept" ) ) {
          target_.sec_websocket_accept = state.second;
        } else if ( header_equals( state.first, "Location" ) ) {
          target_.location = state.second;
        }

        current_line_ = std::move( state );
      }
    }

    return orig_input.size() - input.size();
  };

  State release_extra_state() { return current_line_.release(); }
};

class HTTPResponseReader
{
  HTTPResponse target_;

  bool request_method_was_head_;

  LineReader<PairReader<SpaceReader<StringReader>, PairReader<SpaceReader<StringReader>, StringReader>>>
    status_line_reader_;
  HTTPHeaderReader header_reader_;
  std::optional<LengthReader> body_reader_;

  bool status_line_finished_ {}, headers_finished_ {}, body_finished_ {};

public:
  using State = HTTPHeaderReader::State;

  HTTPResponse release() { return std::move( target_ ); }

  HTTPResponseReader( const bool request_method_was_head, HTTPResponse&& target, State&& state )
    : target_( std::move( target ) )
    , request_method_was_head_( request_method_was_head )
    , status_line_reader_( { std::move( target_.http_version ),
                             { std::move( target_.status_code ), std::move( target.reason_phrase ) } } )
    , header_reader_( std::move( state ) )
    , body_reader_()
  {}

  bool finished() const { return body_finished_; }

  static bool status_code_implies_empty_body( const std::string_view status_code )
  {
    if ( status_code.length() != 3 ) {
      throw std::runtime_error( "bad status code: " + std::string( status_code ) );
    }

    return status_code.front() == '1' or status_code == std::string_view( "204" )
           or status_code == std::string_view( "304" );
  }

  size_t read( const std::string_view orig_input )
  {
    std::string_view input = orig_input;

    while ( ( not finished() ) and ( not input.empty() ) ) {
      if ( not status_line_finished_ ) {
        input.remove_prefix( status_line_reader_.read( input ) );
        if ( status_line_reader_.finished() ) {
          status_line_finished_ = true;
          std::forward_as_tuple( target_.http_version, std::tie( target_.status_code, target_.reason_phrase ) )
            = status_line_reader_.release();
        } else {
          continue;
        }
      }

      if ( not headers_finished_ ) {
        input.remove_prefix( header_reader_.read( input ) );
        if ( header_reader_.finished() ) {
          headers_finished_ = true;
          target_.headers = header_reader_.release();
        } else {
          continue;
        }
      }

      if ( not body_reader_.has_value() ) {
        /* do we know the length of the body? */
        if ( request_method_was_head_ or status_code_implies_empty_body( target_.status_code ) ) {
          body_reader_.emplace( std::move( target_.body ), 0 );
        } else if ( target_.headers.content_length.has_value() ) {
          body_reader_.emplace( std::move( target_.body ), target_.headers.content_length.value() );
        } else {
          throw std::runtime_error( "HTTPResponseReader: unknown body length" );
        }
      }

      if ( not body_finished_ ) {
        input.remove_prefix( body_reader_.value().read( input ) );
        if ( body_reader_.value().finished() ) {
          body_finished_ = true;
          target_.body = body_reader_.value().release();
        }
      }
    }

    return orig_input.size() - input.size();
  }

  State release_extra_state() { return header_reader_.release_extra_state(); }
};

class HTTPRequestReader
{
  HTTPRequest target_;

  LineReader<PairReader<SpaceReader<StringReader>, PairReader<SpaceReader<StringReader>, StringReader>>>
    request_line_reader_;
  HTTPHeaderReader header_reader_;
  std::optional<LengthReader> body_reader_;

  bool request_line_finished_ {}, headers_finished_ {}, body_finished_ {};

public:
  using State = HTTPHeaderReader::State;

  HTTPRequest release() { return std::move( target_ ); }

  HTTPRequestReader( HTTPRequest&& target, State&& state )
    : target_( std::move( target ) )
    , request_line_reader_(
        { std::move( target_.method ), { std::move( target_.request_target ), std::move( target.http_version ) } } )
    , header_reader_( std::move( state ) )
    , body_reader_()
  {}

  bool finished() const { return body_finished_; }

  size_t read( const std::string_view orig_input )
  {
    std::string_view input = orig_input;

    while ( ( not finished() ) and ( not input.empty() ) ) {
      if ( not request_line_finished_ ) {
        input.remove_prefix( request_line_reader_.read( input ) );
        if ( request_line_reader_.finished() ) {
          request_line_finished_ = true;
          std::forward_as_tuple( target_.method, std::tie( target_.request_target, target_.http_version ) )
            = request_line_reader_.release();
        } else {
          continue;
        }
      }

      if ( not headers_finished_ ) {
        input.remove_prefix( header_reader_.read( input ) );
        if ( header_reader_.finished() ) {
          headers_finished_ = true;
          target_.headers = header_reader_.release();
        } else {
          continue;
        }
      }

      if ( not body_reader_.has_value() ) {
        /* do we know the length of the body? */
        if ( target_.method == "GET" or target_.method == "HEAD" ) {
          body_reader_.emplace( std::move( target_.body ), 0 );
        } else if ( target_.headers.content_length.has_value() ) {
          body_reader_.emplace( std::move( target_.body ), target_.headers.content_length.value() );
        } else {
          throw std::runtime_error( "HTTPRequestReader: unknown body length" );
        }
      }

      if ( not body_finished_ ) {
        input.remove_prefix( body_reader_.value().read( input ) );
        if ( body_reader_.value().finished() ) {
          body_finished_ = true;
          target_.body = body_reader_.value().release();
        }
      }
    }

    return orig_input.size() - input.size();
  }

  State release_extra_state() { return header_reader_.release_extra_state(); }
};
