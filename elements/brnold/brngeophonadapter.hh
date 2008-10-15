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

#ifndef BRNGEOPHONADAPTER_HH_
#define BRNGEOPHONADAPTER_HH_

#include <click/element.hh>
#include <click/timer.hh>
#include <termios.h>
CLICK_DECLS

/*
 * =c
 * BRNGeophonAdapter([])
 * =d
 * 
 * This element gathers data from the geophon sensor.
 *
 */

class BRNGeophonAdapter : public Element {
public:
	BRNGeophonAdapter();
	virtual ~BRNGeophonAdapter();

public:
  const char *class_name() const    { return "BRNGeophonAdapter"; }
  const char *port_count() const                { return "0/0-1"; }
  const char *processing() const    { return PUSH; }
  //Packet * simple_action(Packet *p_in);

  int configure(Vector<String> &conf, ErrorHandler *errh);
  int initialize(ErrorHandler *);
  void uninitialize();
  void cleanup(CleanupStage);
  void add_handlers();

  void selected(int);

  inline void handle_text(const String &message);
  void flush();

private:
  static void on_gather_expired(Timer *, void *);

// Serial stuff...
private:
  int setup_serial(ErrorHandler *errh);
  void shutdown_serial();
  String gather_serial();

// Config params
public:
  int _debug;
  unsigned short _portno;
  String _sSerialPort;
  int _gather_intv;

// Datas
private:
  Timer _gather_timer;

  int _socket_fd;

  Vector<String> _messages;
  Vector<uint32_t> _message_pos;
  uint32_t _max_pos;
  
  Vector<int> _fd_alive;
  Vector<uint32_t> _fd_pos;
  int _live_fds;

  enum { MAX_BACKLOG = 500000 };

  int initialize_socket_error(ErrorHandler *, const char *);
  int initialize_socket(ErrorHandler *);
  
  int flush(int fd);
  void flush_output();
  
  struct termios _oldtty;      // will be used to save old port settings
  int _fd_serial;
};

inline void
BRNGeophonAdapter::handle_text(const String &message)
{
  if (_live_fds && message.length()) {
    _messages.push_back(message);
    _message_pos.push_back(_max_pos);
    _max_pos += message.length();
    flush();
  }
}

CLICK_ENDDECLS
#endif /*BRNGEOPHONADAPTER_HH_*/
