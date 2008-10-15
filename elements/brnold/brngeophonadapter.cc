/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * brngeophonadapter.{cc,hh} -- gathers data from the seismic sensor
 */

#include <click/config.h>
#include "common.hh"
#include "brngeophonadapter.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <clicknet/tcp.h> /* for SEQ_LT, etc. */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif //HAVE_UNISTD_H
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
CLICK_DECLS

#define BAUDRATE B2400      // baud rate the device spits out at

BRNGeophonAdapter::BRNGeophonAdapter() : 
  _debug(BrnLogger::DEFAULT),
  _portno(54321),
  _sSerialPort("/dev/usb/tts/0"),
  _gather_intv(500),
  _gather_timer( on_gather_expired, this),
  _socket_fd(-1)
{
}

BRNGeophonAdapter::~BRNGeophonAdapter()
{
}
/*
Packet *
BRNGateway::simple_action(Packet *p_in)
{
  // create a writeable packet
  WritablePacket *p = p_in->uniqueify();
}
*/

int
BRNGeophonAdapter::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
        cpOptional,
        cpBool, "debug", &_debug,
        cpUnsignedShort, "port number", &_portno,
        cpString, "serial port", &_sSerialPort,
        cpInteger, "gather interval", &_gather_intv,
        cpKeywords,
        "DEBUG", cpInteger, "debug", &_debug,
        "PORT", cpUnsignedShort, "port number", &_portno,
        "SERIAL", cpString, "serial port", &_sSerialPort,
        "INTERVAL", cpInteger, "gather interval", &_gather_intv,
         cpEnd) < 0)
    return -1;

  return 0;
}

int
BRNGeophonAdapter::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
  int e = errno;    // preserve errno

  if (_socket_fd >= 0) {
    close(_socket_fd);
    _socket_fd = -1;
  }

  return errh->error("%s: %s", syscall, strerror(e));
}

int
BRNGeophonAdapter::initialize_socket(ErrorHandler *errh)
{
  // open socket, set options, bind to address
  _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (_socket_fd < 0)
    return initialize_socket_error(errh, "socket");
  int sockopt = 1;
  if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&sockopt, sizeof(sockopt)) < 0)
    errh->warning("setsockopt: %s", strerror(errno));

  // bind to port
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(_portno);
  sa.sin_addr = inet_makeaddr(0, 0);
  if (bind(_socket_fd, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    return initialize_socket_error(errh, "bind");


  // start listening
  if (listen(_socket_fd, 2) < 0)
    return initialize_socket_error(errh, "listen");
  
  // nonblocking I/O and close-on-exec for the socket
  fcntl(_socket_fd, F_SETFD, FD_CLOEXEC);

  add_select(_socket_fd, SELECT_READ);
  return 0;
}

int
BRNGeophonAdapter::initialize(ErrorHandler *errh)
{
  _max_pos = 0;
  _live_fds = 0;

  if (initialize_socket(errh) < 0)
    return -1;

  if( setup_serial(errh) < 0 )
    return -1;

  // constantly gather data
  _gather_timer.initialize(this);
  _gather_timer.schedule_after_msec(_gather_intv);
  
  return( 0 );
}

/* uninitializes error handler */
void
BRNGeophonAdapter::uninitialize()
{
  _gather_timer.unschedule();
  shutdown_serial();
}

void
BRNGeophonAdapter::cleanup(CleanupStage)
{
  if (_socket_fd >= 0) {
    // shut down the listening socket in case we forked
#ifdef SHUT_RDWR
    shutdown(_socket_fd, SHUT_RDWR);
#else
    shutdown(_socket_fd, 2);
#endif
    close(_socket_fd);
    _socket_fd = -1;
  }
  
  for (int i = 0; i < _fd_alive.size(); i++)
    if (_fd_alive[i]) {
      close(i);
      _fd_alive[i] = 0;
    }
  _live_fds = 0;
}

int
BRNGeophonAdapter::flush(int fd)
{
  // check file descriptor
  if (fd >= _fd_alive.size() || !_fd_alive[fd])
    return _messages.size();

  // check if all data written
  if (_fd_pos[fd] == _max_pos)
    return _messages.size();

  // find first useful message (binary search)
  uint32_t fd_pos = _fd_pos[fd];
  int l = 0, r = _messages.size() - 1, useful_message = -1;
  while (l <= r) 
  {
    int m = (l + r) >> 1;
    if (SEQ_LT(fd_pos, _message_pos[m]))
      r = m - 1;
    else if (SEQ_GEQ(fd_pos, _message_pos[m] + _messages[m].length()))
      l = m + 1;
    else {
      useful_message = m;
      break;
    }
  }

  // if messages found, write data until blocked or closed
  if (useful_message >= 0) 
  {
    while (useful_message < _message_pos.size()) 
    {
      const String &m = _messages[useful_message];
      int mpos = _message_pos[useful_message];
      const char *data = m.data() + (fd_pos - mpos);
      int len = m.length() - (fd_pos - mpos);
      int w = write(fd, data, len);

      if (w < 0 && errno != EINTR) 
      {
        if (errno != EAGAIN)  // drop connection on error, except WOULDBLOCK
          useful_message = -1;
        break;
      } 
      else if (w > 0)
        fd_pos += w;

      if (SEQ_GEQ(fd_pos, mpos + m.length()))
        useful_message++;
    }
  }

  // store changed fd_pos
  _fd_pos[fd] = fd_pos;
  
  // close out on error, or if socket falls too far behind
  if (useful_message < 0 || SEQ_LT(fd_pos, _max_pos - MAX_BACKLOG)) 
  {
    close(fd);
    remove_select(fd, SELECT_WRITE);
    _fd_alive[fd] = 0;
    _live_fds--;
  } 
  else if (fd_pos == _max_pos)
    remove_select(fd, SELECT_WRITE);
  else
    add_select(fd, SELECT_WRITE);
  
  return useful_message;
}

void
BRNGeophonAdapter::flush_output()
{
  if( 0 >= noutputs() )
    return;
  
  //WritablePacket* p = Packet::make(len);
  //output(0)->push(p);
  /// TODO
}

void
BRNGeophonAdapter::flush()
{
  int min_useful_message = _messages.size();
  if (min_useful_message)
  {
    flush_output();

    for (int i = 0; i < _fd_alive.size(); i++)
    {
      if (_fd_alive[i] >= 0) 
      {
        int m = flush(i);
        if (m < min_useful_message)
        {
          min_useful_message = m;
        }
      }
    }
  }
  
  // cull old messages
  if (min_useful_message >= 10) {
    _messages.erase(_messages.begin(), _messages.begin() + min_useful_message);
    _message_pos.erase(_message_pos.begin(), _message_pos.begin() + min_useful_message);
  }
}

void
BRNGeophonAdapter::selected(int fd)
{
  if (fd == _socket_fd) {
    union { struct sockaddr_in in; struct sockaddr_un un; } sa;
#if HAVE_ACCEPT_SOCKLEN_T
    socklen_t sa_len;
#else
    int sa_len;
#endif
    sa_len = sizeof(sa);
    int new_fd = accept(_socket_fd, (struct sockaddr *)&sa, &sa_len);

    if (new_fd < 0) {
      if (errno != EAGAIN)
        BRN_FATAL("%s: accept: %s", declaration().c_str(), strerror(errno));
      return;
    }
    
    fcntl(new_fd, F_SETFD, FD_CLOEXEC);

    while (new_fd >= _fd_alive.size()) {
      _fd_alive.push_back(0);
      _fd_pos.push_back(0);
    }
    _fd_alive[new_fd] = 1;
    _fd_pos[new_fd] = _max_pos;
    _live_fds++;

    fd = new_fd;
    // no need to SELECT_WRITE; flush(fd) will do it if required
  }

  flush(fd);
}

void 
BRNGeophonAdapter::on_gather_expired(Timer *pTimer, void * pThat)
{
  BRNGeophonAdapter* pThis = (BRNGeophonAdapter*) pThat;
  
  String value = pThis->gather_serial();
  pThis->handle_text(value);
/*
  if(pThis->_debug)
  {
    BRN_DEBUG(value.c_str());
  }
*/
  pTimer->schedule_after_msec(pThis->_gather_intv);
}

int 
BRNGeophonAdapter::setup_serial(ErrorHandler *errh)
{
  struct termios tty;      // will be used for new port settings

  _fd_serial = open( _sSerialPort.c_str(), O_RDWR | O_NOCTTY );
  if (_fd_serial < 0) 
  {
    errh->error("Unable to open serial port (%s), are you root?\n", 
      _sSerialPort.c_str());
    return( -1 );
  }
     
  tcgetattr( _fd_serial, &_oldtty );      // save current port settings
  bzero(&tty, sizeof(tty));      // Initialize the port settings structure to all zeros
  tty.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD | CRTSCTS;      // 8N1
  tty.c_iflag = IGNPAR;
  tty.c_oflag = 0;
  tty.c_lflag = 0;
  tty.c_cc[VMIN] = 0;      // 0 means use-vtime
  tty.c_cc[VTIME] = 1;      // time to wait until exiting read (tenths of a second)

  tcflush( _fd_serial, TCIFLUSH );      // flush old data
  tcsetattr( _fd_serial, TCSANOW, &tty );      // apply new settings
  
  return( 0 );
}

void 
BRNGeophonAdapter::shutdown_serial()
{
  // restore the old port settings before quitting
  tcsetattr( _fd_serial, TCSANOW, &_oldtty );      
}

String 
BRNGeophonAdapter::gather_serial()
{
  // max chars to read at once, if you don't read the entire buffer then the function will get called again
  char temp_buffer[256*2];      
  int len = sizeof(temp_buffer);
  String sRet("");
  
  while( len == sizeof(temp_buffer) )
  {
    // do the actual read
    len = read( _fd_serial, temp_buffer, sizeof(temp_buffer));      
    
    if( 0 > len )
    {
      BRN_ERROR("Unable to gather from serial (%s).", strerror(errno) );
      break;
    }
    if( 0 == len )
      break;
  
    // null terminate the string **important**
    temp_buffer[len] = 0;      

    sRet += String(temp_buffer);
  }
  
  return( sRet );
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

static String
read_debug_param(Element *e, void *)
{
  BRNGeophonAdapter *ds = (BRNGeophonAdapter *)e;
  return String(ds->_debug) + "\n";
}

static int 
write_debug_param(const String &in_s, Element *e, void *,
		      ErrorHandler *errh)
{
  BRNGeophonAdapter *ds = (BRNGeophonAdapter *)e;
  String s = cp_uncomment(in_s);
  int debug;
  if (!cp_integer(s, &debug)) 
    return errh->error("debug parameter must be an integer value between 0 and 4");
  ds->_debug = debug;
  return 0;
}

void
BRNGeophonAdapter::add_handlers()
{
  add_read_handler("debug", read_debug_param, 0);
  add_write_handler("debug", write_debug_param, 0);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(BRNGeophonAdapter)
