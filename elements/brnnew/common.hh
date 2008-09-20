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

#ifndef COMMON_HH
#define COMMON_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>
#include <click/string.hh>
#include <click/straccum.hh>
#include "brn.h"
//BRNNEW #include "brnlinktable.hh"
#include <stdarg.h>

CLICK_DECLS

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) ((void)P)
#endif 

#ifndef min
#define min(x,y)      ((x)<(y) ? (x) : (y))
#endif

#ifndef max
#define max(x,y)      ((x)>(y) ? (x) : (y))
#endif 

#ifdef CLICK_LINUXMODULE

#warning "Click_LinuxModule: BRN_LOG etc. is defined as click_chatter"
#define BRN_LOG   click_chatter
#define BRN_FATAL click_chatter
#define BRN_ERROR click_chatter
#define BRN_WARN  click_chatter
#define BRN_INFO  click_chatter
#define BRN_DEBUG click_chatter

#else
#warning "No Click_LinuxModule"

// TODO check if log level of this element is appropriate!
#define BRN_LOG     BrnLogger(__FILE__,__LINE__,this).log

/** 
 * Error occured which leads to an abort or an undefined state. 
 */
#define BRN_FATAL   if (_debug >= BrnLogger::FATAL) BrnLogger(__FILE__,__LINE__,this).fatal

/** 
 * An error occured, but we are able to recover from it. 
 */
#define BRN_ERROR   if (_debug >= BrnLogger::ERROR) BrnLogger(__FILE__,__LINE__,this).error

/**
 * Print out a warn message. Is the default log level.
 */
#define BRN_WARN    if (_debug >= BrnLogger::WARN) BrnLogger(__FILE__,__LINE__,this).warn

/**
 * Print out an info message.
 * Should be coarse information which helps to understand what happens currently.
 */
#define BRN_INFO    if (_debug >= BrnLogger::INFO) BrnLogger(__FILE__,__LINE__,this).info

/**
 * Print out a debug message.
 * Should be detailed information like function enter/leave etc.
 */
#define BRN_DEBUG   if (_debug >= BrnLogger::DEBUG) BrnLogger(__FILE__,__LINE__,this).debug

#endif

#define BRN_CHECK_EXPR(expr,err)                                       \
{                                                                      \
  if (expr)                                                            \
  {                                                                    \
    BRN_ERROR err;                                                     \
  }                                                                    \
}

#define BRN_CHECK_EXPR_RETURN(expr,err,cleanup)                        \
{                                                                      \
  if (expr)                                                            \
  {                                                                    \
    BRN_ERROR err;                                                     \
    cleanup;                                                           \
  }                                                                    \
}
#ifdef FATAL
#undef FATAL
#endif
#ifdef ERROR
#undef ERROR
#endif

// TODO inline functions
// TODO make buffers dynamic
// NOTE not mt-safe
class BrnLogger
{
public:
  enum {
    FATAL     = 0,
    ERROR     = 1,
    WARN      = 2,
    INFO      = 3,
    DEBUG     = 4,
    
    DEFAULT   = 2 
  };
public:
  inline BrnLogger(
    const char* file, 
    int line, 
    const Element* elem);

  inline BrnLogger(
    const char* file, 
    int line, 
    const Timestamp& time, 
    const String& id, 
    const String& name);
  
public:
  inline void log(int level, const char* format, ...) const;
  inline void fatal(const char* format, ...) const;
  inline void error(const char* format, ...) const;
  inline void warn(const char* format, ...) const;
  inline void info(const char* format, ...) const;
  inline void debug(const char* format, ...) const;

protected:
  void log(int level, const char* format, va_list ptr) const;
  
  static const String& get_id(const Element* elem);
  
  static inline String& get_na();

protected:
  const char*     _file;
  int             _line;
  const Timestamp _time;
  const String    _id;
  const String    _name;
  
  static char* _buffer;
  static int   _buffer_len;
  static HashMap<void*,String>* _id_map;
  static String* _s_na;
};

inline BrnLogger::BrnLogger(
  const char* file, 
  int line, 
  const Element* elem) : 
    _file(file),
    _line(line),
    _time(Timestamp::now()),
    _id(BrnLogger::get_id(elem)),
    _name((NULL == elem) ? get_na() : elem->name())
{
}

inline BrnLogger::BrnLogger(
  const char* file, 
  int line, 
  const Timestamp& time, 
  const String& id, 
  const String& name) :
    _file(file),
    _line(line),
    _time(time),
    _id(id),
    _name(name)
{
}

inline String& 
BrnLogger::get_na()
{
  if (NULL == _s_na)
    _s_na = new String("n/a");
  
  return (*_s_na);
}

inline void 
BrnLogger::log(int level, const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(level, format, ptr);
  va_end(ptr);
}

inline void 
BrnLogger::fatal(const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(FATAL, format, ptr);
  va_end(ptr);
}

inline void 
BrnLogger::error(const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(ERROR, format, ptr);
  va_end(ptr);
}

inline void 
BrnLogger::warn(const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(WARN, format, ptr);
  va_end(ptr);
}

inline void 
BrnLogger::info(const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(INFO, format, ptr);
  va_end(ptr);
}

inline void 
BrnLogger::debug(const char* format, ...) const
{
  va_list ptr; 
  va_start(ptr, format);
  log(DEBUG, format, ptr);
  va_end(ptr);
}


/*
 * Common structures, classes, etc.
 */
#define BRN_DSR_MEMORY_MEDIUM_METRIC        1     // static metric for in memory links
#define BRN_DSR_WIRED_MEDIUM_METRIC        10     // static metric for wired links
#define BRN_DSR_WIRELESS_MEDIUM_METRIC    100     // static metric for wireless links

#define BRN_DSR_STATION_METRIC            100  ///< metric for assoc'd stations
#define BRN_DSR_ROAMED_STATION_METRIC    5000  ///< metric for assoc'd stations


/*
 * This class represents a hop (node ethernet address, metric) in a source route.
 */
class RouteQuerierHop
{
  public:
    EtherAddress ether_address;
    IPAddress ip_address;
    uint16_t _metric;

#define BRN_DSR_INVALID_HOP_METRIC 0xFF
#define BRN_DSR_INVALID_ROUTE_METRIC 9999
#define BRN_DSR_MIN_METRIC_RREQ_FWD 1000
#define BRN_DSR_MIN_LINK_METRIC_WITHIN_ROUTE 1000

    RouteQuerierHop(EtherAddress ether, uint16_t c) : ether_address(ether), ip_address(), _metric(c) {}
    RouteQuerierHop(EtherAddress ether, IPAddress ip, uint16_t c) : ether_address(ether), ip_address(ip), _metric(c) {}

    RouteQuerierHop(hwaddr ether) : ether_address(ether.data), ip_address(), _metric(BRN_DSR_INVALID_HOP_METRIC) {}
    RouteQuerierHop(hwaddr ether, struct in_addr ip) : ether_address(ether.data), ip_address(ip), _metric(BRN_DSR_INVALID_HOP_METRIC) {}

    RouteQuerierHop(hwaddr ether, uint16_t c) : ether_address(ether.data), ip_address(), _metric(c) {}
    RouteQuerierHop(hwaddr ether, struct in_addr ip, uint16_t c) : ether_address(ether.data), ip_address(ip), _metric(c) {}

    EtherAddress ether() const { return (const EtherAddress)ether_address; }
    IPAddress ip() const { return (const IPAddress)ip_address; }
};

// info about route requests we're forwarding (if waiting for a
// unidirectionality test result) and those we've forwarded lately.
// kept in a hash table indexed by (src,target,id).  these entries
// expire after some time.
class ForwardedReqKey
{
public:
  EtherAddress _src;
  EtherAddress _target;
  unsigned int _id;

  ForwardedReqKey(EtherAddress src, EtherAddress target, unsigned int id) {
    _src = src; _target = target; _id = id;
    check();
  }
  ForwardedReqKey() {
    // need this for bighashmap::pair to work
  }
  bool operator==(const ForwardedReqKey &f1) {
    check();
    f1.check();
    return ((_src    == f1._src) &&
            (_target == f1._target) &&
            (_id     == f1._id));
  }
  void check() const {
    assert(_src);
    assert(_target);
    assert(_src != _target);
  }
};

class ForwardedReqVal {
public:
  timeval _time_forwarded;

  unsigned short best_metric; // best metric we've forwarded so far

  // the following two variables are set if we're waiting for a
  // unidirectionality test (RREQ with TTL 1) to come back
  timeval _time_unidtest_issued;
  Packet *p;

  void check() const {
//    assert(_time_forwarded.tv_usec > 0); //robert
//    assert(_time_forwarded.tv_sec > 0);  //robert
    assert(best_metric > 0);
  }
};

#define BRN_DSR_RREQ_TIMEOUT 600000 // how long before we timeout entries (ms)
#define BRN_DSR_RREQ_EXPIRE_TIMER_INTERVAL 15000 // how often to check (ms)

typedef HashMap<ForwardedReqKey, ForwardedReqVal> ForwardedReqMap;
typedef ForwardedReqMap::iterator FWReqIter;

class StringTokenizer {
 public:
  StringTokenizer(const String &s) {
    buf = new char[s.length()];
    memcpy(buf, s.data(), s.length());
    curr_ptr = buf;
    length = s.length();
  }

  bool hasMoreTokens() const {
    return curr_ptr < (buf + length);
  }

  ~StringTokenizer() {
    delete buf;
  }

  String getNextToken() {
    char tmp_buf[256];
    memset(tmp_buf, '\0', 256);
    int currIndex = 0;

    while ( curr_ptr < (buf + length) ) {
      if (*curr_ptr == ' ' || *curr_ptr == '\n') {
        curr_ptr++;
        break;
      }
      tmp_buf[currIndex++] = *curr_ptr;
      curr_ptr++;
    }
    tmp_buf[currIndex] = '\0';

    String s(tmp_buf);
    return s;
  }

private:
  char *buf;
  char *curr_ptr;
  int length;
};

extern "C" {
  static inline int etheraddr_sorter(const void *va, const void *vb) {
    EtherAddress *a = (EtherAddress *)va, *b = (EtherAddress *)vb;

    if (a == b) {
      return 0;
    } 

    uint8_t * addr_a = (uint8_t *)a->data();
    uint8_t * addr_b = (uint8_t *)b->data();

    for (int i = 0; i < 6; i++) {
      if (addr_a[i] < addr_b[i])
        return -1;
      else if (addr_a[i] > addr_b[i])
        return 1;
    }
    return 0;
  }
}

static inline void
print_packet(Packet *p, const String& label, unsigned  bytes)
{
    StringAccum sa(label.length() + 2 // label:
       + 6    // (processor)
       + 28   // timestamp:
       + 9    // length |
       + Packet::USER_ANNO_SIZE*2 + 3 // annotations |
       + 3 * bytes);
    if (sa.out_of_memory()) {
  click_chatter("no memory for Print");
  return;
    }

  sa << label;
#ifdef CLICK_LINUXMODULE
  //if (_cpu)
  //  sa << '(' << click_current_processor() << ')';
#endif
  if (label)
    sa << ": ";

  // sa.reserve() must return non-null; we checked capacity above
  int len;
  len = sprintf(sa.reserve(9), "%4d | ", p->length());
  sa.forward(len);

  char *buf = sa.data() + sa.length();
  int pos = 0;

  for (unsigned i = 0; i < bytes && i < p->length(); i++) {
    sprintf(buf + pos, "%02x", p->data()[i] & 0xff);
    pos += 2;
    if ((i % 4) == 3) buf[pos++] = ' ';
  }
  sa.forward(pos);

  click_chatter("%s", sa.c_str());
}


CLICK_ENDDECLS
#endif
