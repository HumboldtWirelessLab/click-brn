#include <click/config.h>
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/handlercall.hh>

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "guiconnector.hh"

CLICK_DECLS

GuiConnector::GuiConnector()
  : _timer(this),
    _fd(-1),_local_port(0),
    _sndbuf(-1), _rcvbuf(-1)
{
  BRNElement::init();
}

GuiConnector::~GuiConnector()
{
  finish();
}

int
GuiConnector::configure(Vector<String> &conf, ErrorHandler *errh)
{
  Args args = Args(this, errh).bind(conf);
  _protocol = IPPROTO_TCP;
  _family = AF_INET;
  _socktype = SOCK_STREAM;

  if (args.read_mp("ADDR", _remote_ip)
          .read_mp("PORT", IPPortArg(_protocol), _remote_port)
          .read_p("DEBUG", _debug)
          .complete() < 0)
      return -1;

  return 0;
}


int
GuiConnector::initialize_socket_error(ErrorHandler * /*errh*/, const char * /*syscall*/)
{
  return 0;
}

int
GuiConnector::initialize(ErrorHandler *errh)
{
  _timer.initialize(this);

  // open socket, set options
  _fd = socket(_family, _socktype, _protocol);

  if (_fd < 0)
    return initialize_socket_error(errh, "socket");

  if (_family == AF_INET) {
    _remote.in.sin_family = _family;
    _remote.in.sin_port = htons(_remote_port);
    _remote.in.sin_addr = _remote_ip.in_addr();
    _remote_len = sizeof(_remote.in);
    _local.in.sin_family = _family;
    _local.in.sin_port = htons(_local_port);
    _local.in.sin_addr = _local_ip.in_addr();
    _local_len = sizeof(_local.in);
  }

  // disable Nagle algorithm
  int _nodelay = 1;
  if (_protocol == IPPROTO_TCP && _nodelay)
    if (setsockopt(_fd, IP_PROTO_TCP, TCP_NODELAY, &_nodelay, sizeof(_nodelay)) < 0)
      return initialize_socket_error(errh, "setsockopt(TCP_NODELAY)");

  // set socket send buffer size
  if (_sndbuf >= 0)
    if (setsockopt(_fd, SOL_SOCKET, SO_SNDBUF, &_sndbuf, sizeof(_sndbuf)) < 0)
      return initialize_socket_error(errh, "setsockopt(SO_SNDBUF)");

  // set socket receive buffer size
  if (_rcvbuf >= 0)
    if (setsockopt(_fd, SOL_SOCKET, SO_RCVBUF, &_rcvbuf, sizeof(_rcvbuf)) < 0)
      return initialize_socket_error(errh, "setsockopt(SO_RCVBUF)");

  if (connect(_fd, (struct sockaddr *)&_remote, _remote_len) < 0) {
    close(_fd);
    _fd = -1;
    return initialize_socket_error(errh, "connect");
  }

  BRN_DEBUG("%s: opened connection %d to %s:%d", declaration().c_str(), _fd,
                    IPAddress(_remote.in.sin_addr).unparse().c_str(), ntohs(_remote.in.sin_port));

  _active = _fd;

    // nonblocking I/O and close-on-exec for the socket
  fcntl(_fd, F_SETFL, O_NONBLOCK);
  fcntl(_fd, F_SETFD, FD_CLOEXEC);

  String out_str = "init";
  int len = write(_active, out_str.data(), out_str.length());
  if ( len < 0 ) BRN_WARN("Write error");

  communicationLoop();

  BRN_DEBUG("init fin");
  return 0;
}

void
GuiConnector::run_timer(Timer* )
{
  BRN_DEBUG("RunTimer");

  String out_str = "schedule";
  int len = write(_active, out_str.data(), out_str.length());
  if ( len < 0 ) BRN_WARN("Write error");

  communicationLoop();
}

void
GuiConnector::cleanup(CleanupStage)
{
  finish();
}

void
GuiConnector::finish()
{
  if ( _fd > 0 ) {
    BRN_DEBUG("Fin");
    String out_str = "finish";
    _timer.unschedule();
    int len = write(_active, out_str.data(), out_str.length());
    
    if ( len < 0 ) BRN_WARN("Write error");

    communicationLoop();

    close(_fd);
    _fd = -1;
  }
}

void
GuiConnector::communicationLoop()
{
  bool runAgain = true;

  while ( runAgain ) {
    int len = read(_active, _buffer, GUI_CONNECTOR_MAX_BUFFER_SIZE);
    if ( len > 0 ) {
      _buffer[len] = '\0';
      BRN_DEBUG("Read %d bytes >%s<",len,_buffer);
      String in_str((char*)_buffer);

      BRN_DEBUG("op: %s",in_str.c_str());

      runAgain = handleOperation(in_str);
    }
/*    else {
      click_chatter("ffooooo");
    }*/
  }
}

bool
GuiConnector::handleOperation(String op)
{
  Vector<String> args;
  cp_spacevec(op, args);

  BRN_DEBUG("op: >%s<",args[0].c_str());

  if ( args[0] == "finish" ) return false;

  if ( args[0] == "schedule" ) {
    BRN_DEBUG("reschedule");
    int time;
    cp_integer(args[1], &time);
    BRN_DEBUG("set reschedule: %d" , time);
    _timer.schedule_after_msec(time);
    return false;
  }

  if ( args[0] == "read" ) {
    BRN_DEBUG("read");
    String out_str = HandlerCall::call_read(args[1], router()->root_element(), ErrorHandler::default_handler());
    int len = write(_active, out_str.data(), out_str.length());
    if ( len < 0 ) BRN_WARN("Write error");

    return true;
  }

  if ( args[0] == "write" ) {
    BRN_DEBUG("write");

    String value = "";

    for ( int i = 2; i < args.size(); i++ ) {
      value += args[i] + " ";
    }

    int success = HandlerCall::call_write(args[1], value, this);

    StringAccum sa;
    sa << success;

    String out_str = sa.take_string();
    int len = write(_active, out_str.data(), out_str.length());
    if ( len < 0 ) BRN_WARN("Write error");

    return true;
  }

  return false;

}

enum {
  H_FINISH
};

static int
write_param(const String &/*in_s*/, Element *e, void *vparam,
                ErrorHandler */*errh*/)
{
  GuiConnector *f = (GuiConnector *)e;
  switch((intptr_t)vparam) {
    case H_FINISH:
      f->finish();
      break;
  }
  
  return 0;
}


void
GuiConnector::add_handlers()
{
  BRNElement::add_handlers();

  add_write_handler("finish", write_param, (void *) H_FINISH);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ns)
EXPORT_ELEMENT(GuiConnector)
