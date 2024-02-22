#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

#include <cstring>
#include <iomanip>
#include <iostream>

using namespace std;

EventLoop::EventLoop()
  : _rule_categories()
{
  _rule_categories.reserve( 64 );
  // prevent _rule_categories from being reallocated in middle of wait_next_event
  // (if a rule adds a new category)
}

unsigned int EventLoop::FDRule::service_count() const
{
  return direction == Direction::In ? fd.read_count() : fd.write_count();
}

size_t EventLoop::add_category( const string& name )
{
  if ( _rule_categories.size() >= _rule_categories.capacity() ) {
    throw runtime_error( "maximum categories reached" );
  }

  _rule_categories.push_back( { name, {}, {} } );
  return _rule_categories.size() - 1;
}

EventLoop::BasicRule::BasicRule( const size_t s_category_id,
                                 const InterestT& s_interest,
                                 const CallbackT& s_callback )
  : category_id( s_category_id )
  , interest( s_interest )
  , callback( s_callback )
  , cancel_requested( false )
{}

EventLoop::FDRule::FDRule( BasicRule&& base,
                           FileDescriptor&& s_fd,
                           const Direction s_direction,
                           const CallbackT& s_cancel,
                           const InterestT& s_recover )
  : BasicRule( base )
  , fd( move( s_fd ) )
  , direction( s_direction )
  , cancel( s_cancel )
  , recover( s_recover )
{}

EventLoop::RuleHandle EventLoop::add_rule( const size_t category_id,
                                           const FileDescriptor& fd,
                                           const Direction direction,
                                           const CallbackT& callback,
                                           const InterestT& interest,
                                           const CallbackT& cancel,
                                           const InterestT& recover )
{
  if ( category_id >= _rule_categories.size() ) {
    throw out_of_range( "bad category_id" );
  }

  _fd_rules.emplace_back( make_shared<FDRule>(
    BasicRule { category_id, interest, callback }, fd.duplicate(), direction, cancel, recover ) );

  return _fd_rules.back();
}

EventLoop::RuleHandle EventLoop::add_rule( const size_t category_id,
                                           const CallbackT& callback,
                                           const InterestT& interest )
{
  if ( category_id >= _rule_categories.size() ) {
    throw out_of_range( "bad category_id" );
  }

  _non_fd_rules.emplace_back( make_shared<BasicRule>( category_id, interest, callback ) );

  return _non_fd_rules.back();
}

void EventLoop::RuleHandle::cancel()
{
  const shared_ptr<BasicRule> rule_shared_ptr = rule_weak_ptr_.lock();
  if ( rule_shared_ptr ) {
    rule_shared_ptr->cancel_requested = true;
  }
}

EventLoop::Result EventLoop::wait_next_event( const int timeout_ms )
{
  // first, handle the non-file-descriptor-related rules
  {
    for ( auto it = _non_fd_rules.begin(); it != _non_fd_rules.end(); ) {
      auto& this_rule = **it;
      bool rule_fired = false;

      if ( this_rule.cancel_requested ) {
        it = _non_fd_rules.erase( it );
        continue;
      }

      uint16_t iterations = 0;
      while ( this_rule.interest() ) {
        if ( ++iterations >= 32768 ) {
          throw runtime_error( "EventLoop: busy wait detected: rule \""
                               + _rule_categories.at( this_rule.category_id ).name + "\" is still interested after "
                               + to_string( iterations ) + " iterations" );
        }

        rule_fired = true;
        MultiTimer<Timer::Category::Nonblock> record_timer {
          _rule_categories.at( this_rule.category_id ).timer,
          _rule_categories.at( this_rule.category_id ).timer_cumulative };
        this_rule.callback();
      }

      if ( rule_fired ) {
        return Result::Success; /* only serve one rule on each iteration */
      }

      ++it;
    }
  }

  // now the file-descriptor-related rules. poll any "interested" file descriptors
  vector<pollfd> pollfds {};
  pollfds.reserve( _fd_rules.size() );
  bool something_to_poll = false;

  // set up the pollfd for each rule
  for ( auto it = _fd_rules.begin(); it != _fd_rules.end(); ) { // NOTE: it gets erased or incremented in loop body
    auto& this_rule = **it;

    if ( this_rule.cancel_requested ) {
      //      this_rule.cancel();
      //      if rule is cancelled externally, no need to call the cancellation callback
      //      this makes it easier to cancel rules and delete captured objects right away
      it = _fd_rules.erase( it );
      continue;
    }

    if ( this_rule.direction == Direction::In && this_rule.fd.eof() ) {
      // no more reading on this rule, it's reached eof
      this_rule.cancel();
      it = _fd_rules.erase( it );
      return Result::Success;
    }

    if ( this_rule.fd.closed() ) {
      this_rule.cancel();
      it = _fd_rules.erase( it );
      return Result::Success;
    }

    if ( this_rule.interest() ) {
      pollfds.push_back( { this_rule.fd.fd_num(), static_cast<short>( this_rule.direction ), 0 } );
      something_to_poll = true;
    } else {
      pollfds.push_back( { this_rule.fd.fd_num(), 0, 0 } ); // placeholder --- we still want errors
    }
    ++it;
  }

  // quit if there is nothing left to poll
  if ( not something_to_poll ) {
    return Result::Exit;
  }

  // call poll -- wait until one of the fds satisfies one of the rules (writeable/readable)
  {
    MultiTimer<Timer::Category::WaitingForEvent> record_timer { _waiting, _waiting_cumulative };
    if ( 0 == CheckSystemCall( "poll", ::poll( pollfds.data(), pollfds.size(), timeout_ms ) ) ) {
      return Result::Timeout;
    }
  }

  // go through the poll results
  for ( auto [it, idx] = make_pair( _fd_rules.begin(), size_t( 0 ) ); it != _fd_rules.end(); ++idx ) {
    const auto& this_pollfd = pollfds.at( idx );
    auto& this_rule = **it;

    const auto poll_error = static_cast<bool>( this_pollfd.revents & ( POLLERR | POLLNVAL ) );
    if ( poll_error ) {
      /* recoverable error? */
      if ( not static_cast<bool>( this_pollfd.revents & POLLNVAL ) ) {
        if ( this_rule.recover() ) {
          ++it;
          continue;
        }
      }

      /* see if fd is a socket */
      int socket_error = 0;
      socklen_t optlen = sizeof( socket_error );
      const int ret = getsockopt( this_rule.fd.fd_num(), SOL_SOCKET, SO_ERROR, &socket_error, &optlen );
      if ( ret == -1 and errno == ENOTSOCK ) {
        cerr << "error on polled file descriptor for rule \"" << _rule_categories.at( this_rule.category_id ).name
             << "\"\n";
      } else if ( ret == -1 ) {
        throw unix_error( "getsockopt" );
      } else if ( optlen != sizeof( socket_error ) ) {
        throw runtime_error( "unexpected length from getsockopt: " + to_string( optlen ) );
      } else if ( socket_error ) {
        cerr << "error on polled socket for rule \"" << _rule_categories.at( this_rule.category_id ).name
             << "\": " << strerror( socket_error ) << "\n";
      }

      this_rule.cancel();
      it = _fd_rules.erase( it );
      return Result::Success;
    }

    const auto poll_ready = static_cast<bool>( this_pollfd.revents & this_pollfd.events );
    const auto poll_hup = static_cast<bool>( this_pollfd.revents & POLLHUP );
    if ( poll_hup && this_pollfd.events && !poll_ready ) {
      // if we asked for the status, and the _only_ condition was a hangup, this FD is defunct:
      //   - if it was POLLIN and nothing is readable, no more will ever be readable
      //   - if it was POLLOUT, it will not be writable again
      this_rule.cancel();
      it = _fd_rules.erase( it );
      return Result::Success;
    }

    if ( poll_ready ) {
      MultiTimer<Timer::Category::Nonblock> record_timer {
        _rule_categories.at( this_rule.category_id ).timer,
        _rule_categories.at( this_rule.category_id ).timer_cumulative };
      // we only want to call callback if revents includes the event we asked for
      const auto count_before = this_rule.service_count();
      this_rule.callback();

      if ( count_before == this_rule.service_count() and ( not this_rule.fd.closed() ) and this_rule.interest() ) {
        throw runtime_error( "EventLoop: busy wait detected: rule \""
                             + _rule_categories.at( this_rule.category_id ).name
                             + "\" did not read/write fd and is still interested" );
      }

      return Result::Success; /* only serve one rule on each iteration */
    }

    ++it; // if we got here, it means we didn't call _fd_rules.erase()
  }

  return Result::Success;
}

void EventLoop::summary( ostream& out ) const
{
  out << "EventLoop timing summary\n------------------------\n\n";

  bool something_printed = false;

  auto print_timer = [&]( const unsigned int name_len, const string_view name, const Timer::Record timer ) {
    if ( timer.count == 0 ) {
      return;
    }

    something_printed = true;

    out << "   " << name << ": ";
    out << string( 27 - min( 27U, name_len ), ' ' );
    out << "mean " << timer.total_ticks / timer.count;

    out << "  [" << timer.min_ticks;
    out << "..";
    out << timer.max_ticks;
    out << "]";

    out << " N=" << timer.count;
    out << "\n";
  };

  out << "Last interval:\n";

  for ( const auto& rule : _rule_categories ) {
    const string name = "\033[1;34m" + rule.name + "\033[m";
    const auto& timer = rule.timer;

    print_timer( rule.name.length(), name, timer );
  }

  print_timer( 17, "waiting for event", _waiting );

  if ( not something_printed ) {
    out << "(no events)\n";
  }

  out << "\nCumulative:\n";

  something_printed = false;

  for ( const auto& rule : _rule_categories ) {
    const string name = "\033[1;32m" + rule.name + "\033[m";
    const auto& timer = rule.timer_cumulative;

    print_timer( rule.name.length(), name, timer );
  }

  print_timer( 17, "waiting for event", _waiting_cumulative );

  if ( not something_printed ) {
    out << "(no events)\n";
  }
}

void EventLoop::reset_summary()
{
  _waiting.reset();
  for ( auto& rule : _rule_categories ) {
    rule.timer.reset();
  }
}
