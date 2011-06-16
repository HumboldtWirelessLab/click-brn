/*
 * settxpower.{cc,hh} -- sets wifi txpower annotation on a packet
 * John Bicket
 *
 * Copyright (c) 2003 Massachusetts Institute of Technology
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "brn2_settxpower.hh"
CLICK_DECLS

BrnSetTXPower::BrnSetTXPower()
{
}

BrnSetTXPower::~BrnSetTXPower()
{
}

int
BrnSetTXPower::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _power = 0;
    return Args(conf, this, errh).read_p("POWER", _power).complete();
}

Packet *
BrnSetTXPower::simple_action(Packet *p_in)
{
  if (p_in) {
    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p_in);
    ceh->magic = WIFI_EXTRA_MAGIC;
    ceh->power = _power;
    return p_in;
  }
  return 0;
}

enum {H_POWER, H_SETPOWER};

int
BrnSetTXPower::set_power_iwconfig(const String &devname, int power, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << devname << " txpower " << power;
  String cmd = cmda.take_string();

  click_chatter("SetPower command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", errh);
  if (out)
    click_chatter("%s: %s", cmd.c_str(), out.c_str());
#endif

  _power = power;

  return 0;
}


static String
BrnSetTXPower_read_param(Element *e, void *thunk)
{
  BrnSetTXPower *td = (BrnSetTXPower *)e;
  switch ((uintptr_t) thunk) {
  case H_POWER:
    return String(td->_power) + "\n";
  default:
    return String();
  }
}
static int
BrnSetTXPower_write_param(const String &in_s, Element *e, void *vparam,
      ErrorHandler *errh)
{
  BrnSetTXPower *f = (BrnSetTXPower *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
    case H_POWER: {
      unsigned m;
      if (!IntArg().parse(s, m))
        return errh->error("power parameter must be unsigned");
      f->_power = m;
      break;
    }
    case H_SETPOWER: {
      Vector<String> args;
      cp_spacevec(s, args);

      String dev = args[0];
      int power;
      if (!cp_integer(args[1], &power))
        return errh->error("power parameter must be unsigned");

      f->set_power_iwconfig(dev, power, errh);
      break;
    }
  }
  return 0;
}

void
BrnSetTXPower::add_handlers()
  {
  add_read_handler("power", BrnSetTXPower_read_param, (void *) H_POWER);
  add_write_handler("power", BrnSetTXPower_write_param, (void *) H_POWER);

  add_write_handler("set_power", BrnSetTXPower_write_param, (void *) H_SETPOWER);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SetTXPower)

