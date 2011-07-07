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

#ifndef BRNLOGGER_HH
#define BRNLOGGER_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/hashmap.hh>
#include <click/timestamp.hh>
#include <click/string.hh>
#include <click/straccum.hh>
#include <stdarg.h>

CLICK_DECLS

#define CLASS_BRN_LOG   if (_debug >= BrnLogger::LOG) click_chatter
#define CLASS_BRN_FATAL if (_debug >= BrnLogger::FATAL) click_chatter
#define CLASS_BRN_ERROR if (_debug >= BrnLogger::ERROR) click_chatter
#define CLASS_BRN_WARN  if (_debug >= BrnLogger::WARN) click_chatter
#define CLASS_BRN_INFO  if (_debug >= BrnLogger::INFO) click_chatter
#define CLASS_BRN_DEBUG if (_debug >= BrnLogger::DEBUG) click_chatter

#ifdef CLICK_LINUXMODULE

#warning "Click_LinuxModule: BRN_LOG etc. is defined as click_chatter"
#define BRN_LOG   if (_debug >= BrnLogger::LOG) click_chatter
#define BRN_FATAL if (_debug >= BrnLogger::FATAL) click_chatter
#define BRN_ERROR if (_debug >= BrnLogger::ERROR) click_chatter
#define BRN_WARN  if (_debug >= BrnLogger::WARN) click_chatter
#define BRN_INFO  if (_debug >= BrnLogger::INFO) click_chatter
#define BRN_DEBUG if (_debug >= BrnLogger::DEBUG) click_chatter

#else
//#warning "No Click_LinuxModule"

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

#define BRN_NODE_NAME BrnLogger(__FILE__,__LINE__,this).get_name()
#define BRN_NODE_ADDRESS BrnLogger(__FILE__,__LINE__,this).get_address()

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

  static void chatter(const char *fmt, ...);

  inline String get_name() const;
  inline String get_address() const;

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

public:

  static void destroy() {
    if ( _id_map != NULL ) {
      delete _id_map;
      _id_map = NULL;
    }
  }
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

inline String
BrnLogger::get_address() const
{
  return get_na();
}

inline String
BrnLogger::get_name() const
{
  return (_id);
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

CLICK_ENDDECLS
#endif
