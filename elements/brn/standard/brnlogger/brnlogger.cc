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
 * brnlogger.{cc,hh} -- logger functions
 * M. Kurth
 */

////////////////////////////////////////////////////////////////////////////////

#include <click/config.h>
#include <click/error.hh>
#include <click/router.hh>

#include "brnlogger.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"

CLICK_DECLS

////////////////////////////////////////////////////////////////////////////////

char* BrnLogger::_buffer                  = NULL;
int BrnLogger::_buffer_len                = 128;
HashMap<void*,String>* BrnLogger::_id_map = NULL;
String* BrnLogger::_s_na                  = NULL;

////////////////////////////////////////////////////////////////////////////////

const String&
BrnLogger::get_id(const Element* elem)
{
  if (NULL == _id_map)
    _id_map = new HashMap<void*,String>();

  if (NULL == elem)
    return get_na();

  Router* router = elem->router();
  if (NULL == router)
    return get_na();

  String* pid = _id_map->findp(router);
  if (NULL != pid)
    return (*pid);

  Element* elem2 = NULL;
  const Vector<Element*>&  vecElements = router->elements();
  for (int i = 0; i < vecElements.size(); i++)
  {
    if(String(vecElements[i]->class_name()) != String("BRN2NodeIdentity"))
      continue;

    elem2 = vecElements[i];
    break;
  }


  if (NULL == elem2 || !elem2->cast("BRN2NodeIdentity"))
    return get_na();

  BRN2NodeIdentity* id = reinterpret_cast<BRN2NodeIdentity*>(elem2);
  _id_map->insert(router, id->getMasterAddress()->unparse());

  return (*_id_map->findp(router));
}

////////////////////////////////////////////////////////////////////////////////

bool
BrnLogger::log(int level, const char* format, va_list ptr) const
{
  if (NULL == _buffer)
    _buffer = new char[_buffer_len];

  const char* level_string = "FATAL";
  switch (level) {
  case ERROR:
    level_string = "ERROR";
    break;
  case WARN:
    level_string = "WARN ";
    break;
  case INFO:
    level_string = "INFO ";
    break;
  case DEBUG:
    level_string = "DEBUG";
    break;
  case LOG:
    level_string = "LOG";
    break;
  }

  int buffer_needed = 0;
  buffer_needed = vsnprintf(_buffer, _buffer_len-1, format, ptr);

  // need to reallocate?
  if (buffer_needed >= _buffer_len)
  {
    delete[] _buffer;
    _buffer_len = buffer_needed * 2;
    _buffer = new char[_buffer_len];
    
    return false;
  }
  
  _buffer[_buffer_len-1] = 0;

  StringAccum sa;
  sa << level_string << " " << _time << " (" << _name << "," << _id << ") " << _buffer;

  click_chatter(sa.take_string().c_str());
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void
BrnLogger::chatter(const char *fmt, ...)
{
  va_list val;
  va_start(val, fmt);

#if CLICK_LINUXMODULE
# if __MTCLICK__
    static char buf[NR_CPUS][512];      // XXX
    click_processor_t cpu = click_get_processor();
    int i = vsnprintf(buf[cpu], 512, fmt, val);
    printk("<1>%s", buf[cpu]);
    click_put_processor();
# else
    static char buf[512];               // XXX
    int i = vsnprintf(buf, 512, fmt, val);
    printk("<1>%s", buf);
# endif
#elif CLICK_BSDMODULE
    vprintf(fmt, val);
#else /* User-space */
    vfprintf(stderr, fmt, val);
    //fprintf(stderr, "\n");
#endif

  va_end(val);
}


#include <click/bighashmap.cc>
CLICK_ENDDECLS
ELEMENT_PROVIDES(BrnLogger)

////////////////////////////////////////////////////////////////////////////////
