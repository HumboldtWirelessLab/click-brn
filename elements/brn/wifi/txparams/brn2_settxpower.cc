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

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "brn2_settxpower.hh"
CLICK_DECLS

BrnSetTXPower::BrnSetTXPower():
    _power(0),
    _device(NULL)
{
  BRNElement::init();
}

BrnSetTXPower::~BrnSetTXPower()
{
}

int
BrnSetTXPower::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh).read("DEVICE", ElementCastArg("BRN2Device"), _device)
                                 .read_p("POWER", _power)
                                 .read_p("DEBUG", _debug)
                                 .complete();
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

  if ( devname == "" ) {
    if ( _device == NULL ) {
      return 0;
    } else {
      cmda << " " << _device->getDeviceName();
    }
  } else {
    cmda << " " << devname;
  }

  cmda << " txpower " << power;
  String cmd = cmda.take_string();

  click_chatter("SetPower command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", errh);
  if (out)
    click_chatter("%s: %s", cmd.c_str(), out.c_str());
#endif

  _power = power;

  return 0;
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

      String dev;
      int power_index, power;

      if ( args.size() < 2 ) {
        dev = String("");
        power_index = 0;
      } else {
        dev = args[0];
        power_index = 1;
      }

      if (!cp_integer(args[power_index], &power))
        return errh->error("channel parameter must be integer");

      f->set_power_iwconfig(dev, power, errh);
      break;
    }
  }
  return 0;
}

/* Get Info */

String
BrnSetTXPower::get_info()
{
  StringAccum sa;
  sa << "<settxpower name=\"" << BRN_NODE_NAME << "\" id=\"" << BRN_NODE_NAME << "\" power=\"" << _power  << "\" >\n\t<device name=\"";
  if ( _device != NULL ) {
    sa << _device->getDeviceName() << "\" device_addr=\"" << _device->getEtherAddress()->unparse() << "\" />\n";
  } else {
    sa << "n/a\" device_addr=\"n/a\" />\n";
  }

  sa << "</settxpower>\n";
  return sa.take_string();
}


static String
BrnSetTXPower_read_param(Element *e, void *thunk)
{
  BrnSetTXPower *td = (BrnSetTXPower *)e;
  switch ((uintptr_t) thunk) {
    case H_POWER: {
      return td->get_info();
    }
    default:
      return String();
  }
}

void
BrnSetTXPower::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("power", BrnSetTXPower_read_param, (void *) H_POWER);
  add_write_handler("power", BrnSetTXPower_write_param, (void *) H_POWER);

  add_read_handler("systempower", BrnSetTXPower_read_param, (void *) H_POWER);
  add_write_handler("systempower", BrnSetTXPower_write_param, (void *) H_SETPOWER);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BrnSetTXPower)

