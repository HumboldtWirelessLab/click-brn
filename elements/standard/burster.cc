/*
 * burster.{cc,hh} -- element pulls packets from input, pushes to output
 * in bursts
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "burster.hh"
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

Burster::Burster()
  : _npackets(8), _timer(this)
{
}

Burster::~Burster()
{
}

int
Burster::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
		   "INTERVAL", cpkP+cpkM, cpSecondsAsMilli, &_interval,
		   "BURST", cpkP, cpUnsigned, &_npackets,
		   cpEnd) < 0)
    return -1;
  if (_npackets <= 0)
    return errh->error("max packets per interval must be > 0");
  return 0;
}

int
Burster::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  _timer.schedule_after_msec(_interval);
  return 0;
}

void
Burster::run_timer(Timer *)
{
  // don't run if the timer is scheduled (an upstream queue went empty but we
  // don't care)
  if (_timer.scheduled())
    return;

  for (int i = 0; i < _npackets; i++) {
    Packet *p = input(0).pull();
    if (!p)
      break;
    output(0).push(p);
  }

  // reset timer
  _timer.reschedule_after_msec(_interval);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Burster)
