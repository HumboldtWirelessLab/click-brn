#ifndef CLICK_CHATTERSOCKET_HH
#define CLICK_CHATTERSOCKET_HH
#include <click/element.hh>
#include <click/error.hh>
CLICK_DECLS

/*
=c

ChatterSocket("TCP", PORTNUMBER [, I<KEYWORDS>])
ChatterSocket("UNIX", FILENAME [, I<KEYWORDS>])

=s control

reports chatter messages to connected sockets

=d

Opens a chatter socket that allows other user-level programs to receive copies
of router chatter traffic. Depending on its configuration string,
ChatterSocket will listen on TCP port PORTNUMBER, or on a UNIX-domain socket
named FILENAME.

The "server" (that is, the ChatterSocket element) simply echoes any messages
generated by the router configuration to any existing "clients". The server
does not read any data from its clients.

When a connection is opened, ChatterSocket responds by stating its protocol
version number with a line like "Click::ChatterSocket/1.0\r\n". The current
version number is 1.0.

ChatterSocket broadcasts copies of messages generated by the default
ErrorHandler or the C<click_chatter> function. Most elements report messages
or run-time errors using one of these mechanisms.

If a client falls more than 500,000 bytes behind, ChatterSocket automatically
closes its connection.

ChatterSocket supports hot-swapping, meaning you can change configurations
without interrupting existing clients. The hot-swap will succeed only if the
old ChatterSocket and the new ChatterSocket have the same element name, and
the same socket type, port/filename, and chatter channel parameters.
Additionally, the new ChatterSocket must have RETRIES set to 1 or more, since
the old ChatterSocket has already bound the relevant socket.

Keyword arguments are:

=over 8

=item CHANNEL

Text word. The socket outputs messages sent to this chatter channel. Default
is the default channel, which corresponds to C<click_chatter()>.

Channels help you organize extensive debugging output. For example, you could
send extremely verbose messages to a `C<verbose>' channel, then only connect
to that channel when you want verbosity.

To send messages to a particular channel, you should fetch the ErrorHandler
object corresponding to that channel, using the Router member function
C<Router::chatter_channel(const String &channel_name)>.

=item QUIET_CHANNEL

Boolean. Messages sent to a non-default channel are not normally printed on
standard error. If QUIET_CHANNEL is false, however, the channel's messages do
go to standard error, along with chatter messages. Default is true.

=item GREETING

Boolean. Determines whether the C<Click::ChatterSocket/1.0> greeting is sent.
Default is true.

=item RETRIES

Integer. If greater than 0, ChatterSocket won't immediately fail when it can't
open its socket. Instead, it will attempt to open the socket once a second
until it succeeds, or until RETRIES unsuccessful attempts (after which it will
stop the router). Default is 0.

=item RETRY_WARNINGS

Boolean. If true, ChatterSocket will print warning messages every time it
fails to open a socket. If false, it will print messages only on the final
failure. Default is true.

=back

=e

  ChatterSocket(unix, /tmp/clicksocket);

=a ControlSocket */

class Timer;

class ChatterSocket : public Element { public:

  ChatterSocket();
  ~ChatterSocket();

  const char *class_name() const	{ return "ChatterSocket"; }

  int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
  int configure(Vector<String> &conf, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void take_state(Element *, ErrorHandler *);

  void selected(int);

  void emit(const String &);
  void flush();

 private:

  String _unix_pathname;
  int _socket_fd;
  String _channel;
  bool _greeting : 1;
  bool _retry_warnings : 1;
  bool _tcp_socket : 1;

  Vector<String> _messages;
  Vector<uint32_t> _message_pos;
  uint32_t _max_pos;

  Vector<int> _fd_alive;
  Vector<uint32_t> _fd_pos;
  int _live_fds;

  int _retries;
  Timer *_retry_timer;

  static const char protocol_version[];
  enum { MAX_BACKLOG = 500000 };

  int initialize_socket_error(ErrorHandler *, const char *);
  int initialize_socket(ErrorHandler *);
  static void retry_hook(Timer *, void *);

  int flush(int fd);

};

inline void
ChatterSocket::emit(const String &message)
{
    if (_live_fds && message.length()) {
	_messages.push_back(message);
	_message_pos.push_back(_max_pos);
	_max_pos += message.length();
	flush();
    }
}

CLICK_ENDDECLS
#endif
