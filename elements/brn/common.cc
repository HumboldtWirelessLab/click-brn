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
 * common.{cc,hh} -- common functions
 * M. Kurth
 */

////////////////////////////////////////////////////////////////////////////////

#include <click/config.h>
#include "common.hh"

#include <click/router.hh>
//BRNNEW #include "nodeidentity.hh"
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
    if(String(vecElements[i]->class_name()) != String("NodeIdentity"))
      continue;
      
    elem2 = vecElements[i];
    break;
  }

//BRNNEW
/*  if (NULL == elem2 || !elem2->cast("NodeIdentity"))
    return get_na();
  
  NodeIdentity* id = (NodeIdentity*)elem2;
  _id_map->insert(router,id->getMyWirelessAddress()->s());
  
*/  return (*_id_map->findp(router));
}

////////////////////////////////////////////////////////////////////////////////

void 
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
  }
  
  int buffer_needed = 0;
  buffer_needed = vsnprintf(_buffer, _buffer_len-1, format, ptr);
  
  // need to reallocate?
  if (buffer_needed >= _buffer_len)
  {
    delete[] _buffer;
    while (buffer_needed >= _buffer_len)
      _buffer_len *= 2;
    _buffer = new char[_buffer_len];

    buffer_needed = vsnprintf(_buffer, _buffer_len-1, format, ptr);
    assert(buffer_needed < _buffer_len);
  }
  _buffer[_buffer_len-1] = 0;
  
  StringAccum sa;
  sa << level_string << " " << _time << " (" << _name << "," << _id << ") " << _buffer;
  
  click_chatter(sa.take_string().c_str());
}

////////////////////////////////////////////////////////////////////////////////

#include <click/bighashmap.cc>
CLICK_ENDDECLS
ELEMENT_PROVIDES(brn_common)

////////////////////////////////////////////////////////////////////////////////
