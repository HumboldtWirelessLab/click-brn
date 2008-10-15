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

#ifndef CLICK_TODUMPDLG_HH
#define CLICK_TODUMPDLG_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <stdio.h>
CLICK_DECLS

class ToDump;

/**
 * Delegates dump messages to a real ToDump element.
 */
class ToDumpDlg : public Element { 
public:

  ToDumpDlg();
  ~ToDumpDlg();

  const char *class_name() const	{ return "ToDumpDlg"; }
  const char *port_count() const	{ return "0-1/0-1"; }
  const char *processing() const	{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet* simple_action(Packet*);

private:

  ToDump* _to_dump;
};

CLICK_ENDDECLS
#endif //CLICK_TODUMPDLG_HH
