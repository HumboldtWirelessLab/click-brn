// -*- c-basic-offset: 2; related-file-name: "../include/click/error.hh" -*-
/*
 * error.{cc,hh} -- flexible classes for error reporting
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001-2007 Eddie Kohler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/straccum.hh>
#ifndef CLICK_TOOL
# include <click/element.hh>
#endif
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/timestamp.hh>
#include <click/confparse.hh>
CLICK_DECLS

struct ErrorHandler::Conversion {
  String name;
  ConversionHook hook;
  Conversion *next;
};
static ErrorHandler::Conversion *error_items;

const int ErrorHandler::OK_RESULT = 0;
const int ErrorHandler::ERROR_RESULT = -EINVAL;

int
ErrorHandler::min_verbosity() const
{
  return 0;
}

void
ErrorHandler::debug(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_DEBUG, String(), format, val);
  va_end(val);
}

void
ErrorHandler::message(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_MESSAGE, String(), format, val);
  va_end(val);
}

int
ErrorHandler::warning(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_WARNING, String(), format, val);
  va_end(val);
  return ERROR_RESULT;
}

int
ErrorHandler::error(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_ERROR, String(), format, val);
  va_end(val);
  return ERROR_RESULT;
}

int
ErrorHandler::fatal(const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_FATAL, String(), format, val);
  va_end(val);
  return ERROR_RESULT;
}

void
ErrorHandler::ldebug(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_DEBUG, where, format, val);
  va_end(val);
}

void
ErrorHandler::lmessage(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_MESSAGE, where, format, val);
  va_end(val);
}

int
ErrorHandler::lwarning(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_WARNING, where, format, val);
  va_end(val);
  return ERROR_RESULT;
}

int
ErrorHandler::lerror(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_ERROR, where, format, val);
  va_end(val);
  return ERROR_RESULT;
}

int
ErrorHandler::lfatal(const String &where, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  verror(ERR_FATAL, where, format, val);
  va_end(val);
  return ERROR_RESULT;
}

String
ErrorHandler::make_text(Seriousness seriousness, const char *format, ...)
{
  va_list val;
  va_start(val, format);
  String s = make_text(seriousness, format, val);
  va_end(val);
  return s;
}

#define NUMBUF_SIZE	128
#define ErrH		ErrorHandler

static char *
do_number(unsigned long num, char *after_last, int base, int flags)
{
  const char *digits =
    ((flags & ErrH::UPPERCASE) ? "0123456789ABCDEF" : "0123456789abcdef");
  char *pos = after_last;
  while (num) {
    *--pos = digits[num % base];
    num /= base;
  }
  if (pos == after_last)
    *--pos = '0';
  return pos;
}

static char *
do_number_flags(char *pos, char *after_last, int base, int flags,
		int precision, int field_width)
{
  // remove ALTERNATE_FORM for zero results in base 16
  if ((flags & ErrH::ALTERNATE_FORM) && base == 16 && *pos == '0')
    flags &= ~ErrH::ALTERNATE_FORM;
  
  // account for zero padding
  if (precision >= 0)
    while (after_last - pos < precision)
      *--pos = '0';
  else if (flags & ErrH::ZERO_PAD) {
    if ((flags & ErrH::ALTERNATE_FORM) && base == 16)
      field_width -= 2;
    if ((flags & ErrH::NEGATIVE)
	|| (flags & (ErrH::PLUS_POSITIVE | ErrH::SPACE_POSITIVE)))
      field_width--;
    while (after_last - pos < field_width)
      *--pos = '0';
  }
  
  // alternate forms
  if ((flags & ErrH::ALTERNATE_FORM) && base == 8 && pos[1] != '0')
    *--pos = '0';
  else if ((flags & ErrH::ALTERNATE_FORM) && base == 16) {
    *--pos = ((flags & ErrH::UPPERCASE) ? 'X' : 'x');
    *--pos = '0';
  }
  
  // sign
  if (flags & ErrH::NEGATIVE)
    *--pos = '-';
  else if (flags & ErrH::PLUS_POSITIVE)
    *--pos = '+';
  else if (flags & ErrH::SPACE_POSITIVE)
    *--pos = ' ';
  
  return pos;
}

String
ErrorHandler::make_text(Seriousness seriousness, const char *s, va_list val)
{
  StringAccum msg;

  char numbuf[NUMBUF_SIZE];	// for numerics
  numbuf[NUMBUF_SIZE-1] = 0;

  String placeholder;		// to ensure temporaries aren't destroyed

  // prepend 'warning: ' to every line if appropriate
  String begin_lines;
  if (seriousness >= ERR_MIN_WARNING && seriousness < ERR_MIN_ERROR) {
    begin_lines = String::make_stable("warning: ", 9);
    msg << begin_lines;
  }
  
  // declare and initialize these here to make gcc shut up about possible 
  // use before initialization
  int flags = 0;
  int field_width = -1;
  int precision = -1;
  int width_flag = 0;
  int base = 10;
  while (1) {
    
    const char *pct = strchr(s, '%');

    // handle begin_lines
    const char *nl;
    while (begin_lines && (nl = strchr(s, '\n')) && (!pct || nl < pct)) {
      msg.append(s, nl + 1 - s);
      if (nl[1])
	msg.append(begin_lines.data(), begin_lines.length());
      s = nl + 1;
    }
    
    if (!pct) {
      if (*s)
	msg << s;
      break;
    }
    if (pct != s) {
      msg.append(s, pct - s);
      s = pct;
    }
    
    // parse flags
    flags = 0;
   flags:
    switch (*++s) {
     case '#': flags |= ALTERNATE_FORM; goto flags;
     case '0': flags |= ZERO_PAD; goto flags;
     case '-': flags |= LEFT_JUST; goto flags;
     case ' ': flags |= SPACE_POSITIVE; goto flags;
     case '+': flags |= PLUS_POSITIVE; goto flags;
    }
    
    // parse field width
    field_width = -1;
    if (*s == '*') {
      field_width = va_arg(val, int);
      if (field_width < 0) {
	field_width = -field_width;
	flags |= LEFT_JUST;
      }
      s++;
    } else if (*s >= '0' && *s <= '9')
      for (field_width = 0; *s >= '0' && *s <= '9'; s++)
	field_width = 10*field_width + *s - '0';
    
    // parse precision
    precision = -1;
    if (*s == '.') {
      s++;
      precision = 0;
      if (*s == '*') {
	precision = va_arg(val, int);
	s++;
      } else if (*s >= '0' && *s <= '9')
	for (; *s >= '0' && *s <= '9'; s++)
	  precision = 10*precision + *s - '0';
    }
    
    // parse width flags
    width_flag = 0;
   width_flags:
    switch (*s) {
    case 'h': case 'l':
      if (width_flag == *s)
	width_flag = *s + 'A' - 'a';
      else if (width_flag)
	break;
      else
	width_flag = *s;
      s++;
      goto width_flags;
    case 'z':
      if (width_flag)
	break;
      width_flag = *s++;
      break;
    case '^':
      if (!isdigit((unsigned char) s[1]) || width_flag)
	break;
      for (s++; isdigit((unsigned char) *s); s++)
	width_flag = width_flag * 10 + *s - '0';
      width_flag = -width_flag;
      break;
    }
    
    // conversion character
    // after switch, data lies between `s1' and `s2'
    const char *s1 = 0, *s2 = 0;
    base = 10;
    switch (*s++) {
      
     case 's': {
       s1 = va_arg(val, const char *);
       if (!s1)
	 s1 = "(null)";
       if (flags & ALTERNATE_FORM) {
	 placeholder = String(s1).printable();
	 s1 = placeholder.c_str();
       }
       for (s2 = s1; *s2 && precision != 0; s2++)
	 if (precision > 0)
	   precision--;
       break;
     }

     case 'c': {
       int c = va_arg(val, int);
       // check for extension of 'signed char' to 'int'
       if (c < 0)
	 c += 256;
       // assume ASCII
       if (c == '\n')
	 strcpy(numbuf, "\\n");
       else if (c == '\t')
	 strcpy(numbuf, "\\t");
       else if (c == '\r')
	 strcpy(numbuf, "\\r");
       else if (c == '\0')
	 strcpy(numbuf, "\\0");
       else if (c < 0 || c >= 256)
	 strcpy(numbuf, "(bad char)");
       else if (c < 32 || c >= 0177)
	 sprintf(numbuf, "\\%03o", c);
       else
	 sprintf(numbuf, "%c", c);
       s1 = numbuf;
       s2 = strchr(numbuf, 0);
       break;
     }
     
     case '%': {
       numbuf[0] = '%';
       s1 = numbuf;
       s2 = s1 + 1;
       break;
     }

     case 'd':
     case 'i':
      flags |= SIGNED;
     case 'u':
     number: {
       // protect numbuf from overflow
       if (field_width > NUMBUF_SIZE)
	 field_width = NUMBUF_SIZE;
       if (precision > NUMBUF_SIZE - 4)
	 precision = NUMBUF_SIZE - 4;
       
       s2 = numbuf + NUMBUF_SIZE;
       
       unsigned long num;
       switch (width_flag) {
       case 'H':
       case -8:
	 num = (unsigned char) va_arg(val, int);
	 if ((flags & SIGNED) && (signed char) num < 0)
	   num = -(signed char) num, flags |= NEGATIVE;
	 break;
       case 'h':
       case -16:
	 num = (unsigned short) va_arg(val, int);
	 if ((flags & SIGNED) && (short) num < 0)
	   num = -(short) num, flags |= NEGATIVE;
	 break;
       case 0:
       case -32:
#if SIZEOF_LONG == 4
       case 'l':
#endif
#if SIZEOF_SIZE_T == 4
       case 'z':
#endif
	 num = va_arg(val, unsigned);
	 if ((flags & SIGNED) && (int) num < 0)
	   num = -(int) num, flags |= NEGATIVE;
	 break;
#if HAVE_INT64_TYPES
# if SIZEOF_LONG == 8
       case 'l':
# endif
# if SIZEOF_LONG_LONG == 8
       case 'L':
# endif
# if SIZEOF_SIZE_T == 8
       case 'z':
# endif
       case -64: {
	 uint64_t qnum = va_arg(val, uint64_t);
	 if ((flags & SIGNED) && (int64_t)qnum < 0)
	   qnum = -(int64_t) qnum, flags |= NEGATIVE;
	 StringAccum sa;
	 sa.append_numeric(static_cast<String::uint_large_t>(qnum), base, (flags & UPPERCASE));
	 s1 = s2 - sa.length();
	 memcpy(const_cast<char*>(s1), sa.data(), s2 - s1);
	 goto got_number;
       }
#endif
       default:
	 goto error;
       }
       s1 = do_number(num, (char *)s2, base, flags);

#if HAVE_INT64_TYPES
      got_number:
#endif
       s1 = do_number_flags((char *)s1, (char *)s2, base, flags,
			    precision, field_width);
       break;
     }
     
     case 'o':
      base = 8;
      goto number;
      
     case 'X':
      flags |= UPPERCASE;
     case 'x':
      base = 16;
      goto number;
      
     case 'p': {
       void *v = va_arg(val, void *);
       s2 = numbuf + NUMBUF_SIZE;
       s1 = do_number((unsigned long)v, (char *)s2, 16, flags);
       s1 = do_number_flags((char *)s1, (char *)s2, 16, flags | ALTERNATE_FORM,
			    precision, field_width);
       break;
     }

#if HAVE_FLOAT_TYPES
     case 'e': case 'f': case 'g':
     case 'E': case 'F': case 'G': {
       char format[80], *f = format, new_numbuf[NUMBUF_SIZE];
       *f++ = '%';
       if (flags & ALTERNATE_FORM)
	 *f++ = '#';
       if (precision >= 0)
	 f += sprintf(f, ".%d", precision);
       *f++ = s[-1];
       *f++ = 0;

       int len = sprintf(new_numbuf, format, va_arg(val, double));

       s2 = numbuf + NUMBUF_SIZE;
       s1 = s2 - len;
       memcpy((char *)s1, new_numbuf, len); // note: no terminating \0
       s1 = do_number_flags((char *)s1, (char *)s2, 10, flags & ~ALTERNATE_FORM, -1, field_width);
       break;
     }
#endif
     
     case '{': {
       const char *rbrace = strchr(s, '}');
       if (!rbrace || rbrace == s)
	 assert(0 /* Bad %{ in error */);
       String name(s, rbrace - s);
       s = rbrace + 1;
       for (Conversion *item = error_items; item; item = item->next)
	 if (item->name == name) {
	   placeholder = item->hook(flags, VA_LIST_REF(val));
	   s1 = placeholder.data();
	   s2 = s1 + placeholder.length();
	   goto got_result;
	 }
       goto error;
     }

     error:
     default:
      assert(0 /* Bad % in error */);
      break;
      
    }

    // add result of conversion
   got_result:
    int slen = s2 - s1;
    if (slen > field_width) field_width = slen;
    char *dest = msg.extend(field_width);
    if (flags & LEFT_JUST) {
      memcpy(dest, s1, slen);
      memset(dest + slen, ' ', field_width - slen);
    } else {
      memcpy(dest + field_width - slen, s1, slen);
      memset(dest, (flags & ZERO_PAD ? '0' : ' '), field_width - slen);
    }
  }

  return msg.take_string();
}

String
ErrorHandler::decorate_text(Seriousness, const String &landmark, const String &text)
{
  String new_text = text;

  // skip initial special-purpose portion of a landmark ('\<...>')
  int first_lpos = 0;
  if (landmark.length() > 2 && landmark[0] == '\\' && landmark[1] == '<') {
    int end_special = landmark.find_left('>');
    if (end_special > 0)
      first_lpos = end_special + 1;
  }
  
  // handle landmark
  if (first_lpos < landmark.length()) {
    // fix landmark: skip trailing spaces and trailing colon
    int i, len = landmark.length();
    for (i = len - 1; i >= first_lpos; i--)
      if (!isspace((unsigned char) landmark[i]))
	break;
    if (i >= first_lpos && landmark[i] == ':')
      i--;

    // prepend landmark, unless all spaces
    if (i >= first_lpos)
      new_text = prepend_lines(landmark.substring(first_lpos, i + 1 - first_lpos) + ": ", new_text);
  }

  return new_text;
}

int
ErrorHandler::verror(Seriousness seriousness, const String &where,
		     const char *s, va_list val)
{
  String text = make_text(seriousness, s, val);
  text = decorate_text(seriousness, where, text);
  handle_text(seriousness, text);
  return count_error(seriousness, text);
}

int
ErrorHandler::verror_text(Seriousness seriousness, const String &where,
			  const String &text)
{
  // text is already made
  String dec_text = decorate_text(seriousness, where, text);
  handle_text(seriousness, dec_text);
  return count_error(seriousness, dec_text);
}

void
ErrorHandler::set_error_code(int)
{
}

String
ErrorHandler::prepend_lines(const String &prepend, const String &text)
{
  if (!prepend)
    return text;
  
  StringAccum sa;
  const char *begin = text.begin();
  const char *end = text.end();
  while (begin < end) {
    const char *nl = find(begin, end, '\n');
    if (nl < end)
      nl++;
    sa << prepend << text.substring(begin, nl);
    begin = nl;
  }
  
  return sa.take_string();
}


//
// BASE ERROR HANDLER
//

int
BaseErrorHandler::count_error(Seriousness seriousness, const String &)
{
  if (seriousness < ERR_MIN_WARNING)
    /* do nothing */;
  else if (seriousness < ERR_MIN_ERROR)
    _nwarnings++;
  else
    _nerrors++;
  return (seriousness < ERR_MIN_WARNING ? OK_RESULT : ERROR_RESULT);
}


#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
//
// FILE ERROR HANDLER
//

FileErrorHandler::FileErrorHandler(FILE *f, const String &context)
  : _f(f), _context(context)
{
}

void
FileErrorHandler::handle_text(Seriousness seriousness, const String &message)
{
  String text = prepend_lines(_context, message);
  if (text && text.back() != '\n')
    text += '\n';
  fwrite(text.data(), 1, text.length(), _f);
  
  if (seriousness >= ERR_MIN_FATAL) {
    int exit_status = (seriousness - ERR_MIN_FATAL) >> ERRVERBOSITY_SHIFT;
    exit(exit_status);
  }
}
#endif


//
// SILENT ERROR HANDLER
//

void
SilentErrorHandler::handle_text(Seriousness, const String &)
{
}


//
// STATIC ERROR HANDLERS
//

static ErrorHandler *the_default_handler = 0;
static ErrorHandler *the_silent_handler = 0;

ErrorHandler::Conversion *
ErrorHandler::add_conversion(const String &name, ConversionHook hook)
{
  if (Conversion *c = new Conversion) {
    c->name = name;
    c->hook = hook;
    c->next = error_items;
    error_items = c;
    return c;
  } else
    return 0;
}

int
ErrorHandler::remove_conversion(ErrorHandler::Conversion *conv)
{
  Conversion **pprev = &error_items;
  for (Conversion *c = error_items; c; pprev = &c->next, c = *pprev)
    if (c == conv) {
      *pprev = c->next;
      delete c;
      return 0;
    }
  return -1;
}

static String
timeval_error_hook(int, VA_LIST_REF_T val)
{
  const struct timeval *tvp = va_arg(VA_LIST_DEREF(val), const struct timeval *);
  if (tvp) {
    StringAccum sa;
    sa << *tvp;
    return sa.take_string();
  } else
    return "(null)";
}

static String
timestamp_error_hook(int, VA_LIST_REF_T val)
{
  const Timestamp *tsp = va_arg(VA_LIST_DEREF(val), const Timestamp *);
  if (tsp) {
    StringAccum sa;
    sa << *tsp;
    return sa.take_string();
  } else
    return "(null)";
}

static String
ip_ptr_error_hook(int, VA_LIST_REF_T val)
{
  const IPAddress *ipp = va_arg(VA_LIST_DEREF(val), const IPAddress *);
  if (ipp)
    return ipp->unparse();
  else
    return "(null)";
}

static String
ether_ptr_error_hook(int, VA_LIST_REF_T val)
{
  const EtherAddress *ethp = va_arg(VA_LIST_DEREF(val), const EtherAddress *);
  if (ethp)
    return ethp->unparse();
  else
    return "(null)";
}

#ifndef CLICK_TOOL
static String
element_error_hook(int, VA_LIST_REF_T val)
{
  const Element *e = va_arg(VA_LIST_DEREF(val), const Element *);
  if (e)
    return e->declaration();
  else
    return "(null)";
}
#endif

ErrorHandler *
ErrorHandler::static_initialize(ErrorHandler *default_handler)
{
  the_default_handler = default_handler;
  the_silent_handler = new SilentErrorHandler;
  add_conversion("timeval", timeval_error_hook);
  add_conversion("timestamp", timestamp_error_hook);
#ifndef CLICK_TOOL
  add_conversion("element", element_error_hook);
#endif
  add_conversion("ip_ptr", ip_ptr_error_hook);
  add_conversion("ether_ptr", ether_ptr_error_hook);
  return the_default_handler;
}

void
ErrorHandler::static_cleanup()
{
  delete the_default_handler;
  delete the_silent_handler;
  the_default_handler = the_silent_handler = 0;
  while (error_items) {
    Conversion *next = error_items->next;
    delete error_items;
    error_items = next;
  }
}

bool
ErrorHandler::has_default_handler()
{
  return the_default_handler ? true : false;
}

ErrorHandler *
ErrorHandler::default_handler()
{
  assert(the_default_handler != 0);
  return the_default_handler;
}

ErrorHandler *
ErrorHandler::silent_handler()
{
  assert(the_silent_handler != 0);
  return the_silent_handler;
}

void
ErrorHandler::set_default_handler(ErrorHandler *errh)
{
  the_default_handler = errh;
}


//
// ERROR VENEER
//

int
ErrorVeneer::nwarnings() const
{
  return _errh->nwarnings();
}

int
ErrorVeneer::nerrors() const
{
  return _errh->nerrors();
}

void
ErrorVeneer::reset_counts()
{
  _errh->reset_counts();
}

int
ErrorVeneer::min_verbosity() const
{
  return _errh->min_verbosity();
}

String
ErrorVeneer::make_text(Seriousness seriousness, const char *s, va_list val)
{
  return _errh->make_text(seriousness, s, val);
}

String
ErrorVeneer::decorate_text(Seriousness seriousness, const String &landmark, const String &text)
{
  return _errh->decorate_text(seriousness, landmark, text);
}

void
ErrorVeneer::handle_text(Seriousness seriousness, const String &text)
{
  _errh->handle_text(seriousness, text);
}

int
ErrorVeneer::count_error(Seriousness seriousness, const String &text)
{
  return _errh->count_error(seriousness, text);
}

void
ErrorVeneer::set_error_code(int code)
{
  _errh->set_error_code(code);
}


//
// LOCAL ERROR HANDLER
//

int
LocalErrorHandler::min_verbosity() const
{
  return (_errh ? _errh->min_verbosity() : 0);
}

String
LocalErrorHandler::make_text(Seriousness seriousness, const char *s, va_list val)
{
  return (_errh ? _errh->make_text(seriousness, s, val) : ErrorHandler::make_text(seriousness, s, val));
}

String
LocalErrorHandler::decorate_text(Seriousness seriousness, const String &landmark, const String &text)
{
  return (_errh ? _errh->decorate_text(seriousness, landmark, text) : ErrorHandler::decorate_text(seriousness, landmark, text));
}

void
LocalErrorHandler::handle_text(Seriousness seriousness, const String &text)
{
  if (_errh)
    _errh->handle_text(seriousness, text);
}

int
LocalErrorHandler::count_error(Seriousness seriousness, const String &text)
{
  int val = BaseErrorHandler::count_error(seriousness, text);
  if (_errh)
    return _errh->count_error(seriousness, text);
  else
    return val;
}

void
LocalErrorHandler::set_error_code(int code)
{
  if (_errh)
    _errh->set_error_code(code);
}


//
// CONTEXT ERROR HANDLER
//

ContextErrorHandler::ContextErrorHandler(ErrorHandler *errh,
					 const String &context,
					 const String &indent,
					 const String &context_landmark)
  : ErrorVeneer(errh), _context(context), _indent(indent),
    _context_landmark(context_landmark)
{
}

String
ContextErrorHandler::decorate_text(Seriousness seriousness, const String &landmark, const String &text)
{
  String context_lines;
  if (_context) {
    // do not print context or indent if underlying ErrorHandler doesn't want
    // context
    if (_errh->min_verbosity() > ERRVERBOSITY_CONTEXT)
      _context = _indent = String();
    else {
      context_lines = _errh->decorate_text(ERR_MESSAGE, (_context_landmark ? _context_landmark : landmark), _context);
      if (context_lines && context_lines.back() != '\n')
	context_lines += '\n';
      _context = String();
    }
  }
  return context_lines + _errh->decorate_text(seriousness, (landmark ? landmark : _context_landmark), prepend_lines(_indent, text));
}


//
// PREFIX ERROR HANDLER
//

PrefixErrorHandler::PrefixErrorHandler(ErrorHandler *errh,
				       const String &prefix)
  : ErrorVeneer(errh), _prefix(prefix)
{
}

String
PrefixErrorHandler::decorate_text(Seriousness seriousness, const String &landmark, const String &text)
{
  return _errh->decorate_text(seriousness, landmark, prepend_lines(_prefix, text));
}


//
// LANDMARK ERROR HANDLER
//

LandmarkErrorHandler::LandmarkErrorHandler(ErrorHandler *errh, const String &landmark)
  : ErrorVeneer(errh), _landmark(landmark)
{
}

String
LandmarkErrorHandler::decorate_text(Seriousness seriousness, const String &lm, const String &text)
{
  if (lm)
    return _errh->decorate_text(seriousness, lm, text);
  else
    return _errh->decorate_text(seriousness, _landmark, text);
}


//
// VERBOSE FILTER ERROR HANDLER
//

VerboseFilterErrorHandler::VerboseFilterErrorHandler(ErrorHandler *errh, int min_verbosity)
  : ErrorVeneer(errh), _min_verbosity(min_verbosity)
{
}

int
VerboseFilterErrorHandler::min_verbosity() const
{
  int m = _errh->min_verbosity();
  return (m >= _min_verbosity ? m : _min_verbosity);
}

void
VerboseFilterErrorHandler::handle_text(Seriousness s, const String &text)
{
  if ((s & ERRVERBOSITY_MASK) >= _min_verbosity)
    _errh->handle_text(s, text);
}


//
// BAIL ERROR HANDLER
//

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)

BailErrorHandler::BailErrorHandler(ErrorHandler *errh, Seriousness s)
  : ErrorVeneer(errh), _exit_seriousness(s)
{
}

void
BailErrorHandler::handle_text(Seriousness s, const String &text)
{
  _errh->handle_text(s, text);
  if (s >= _exit_seriousness)
    exit(1);
}

#endif

CLICK_ENDDECLS
