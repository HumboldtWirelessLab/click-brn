/*
 * simevice.{cc,hh} -- element reads packets from a simulated network
 * interface.
 *
 */

#include <click/config.h>

#include <click/simclick.h>
#include "simdevice.hh"

CLICK_DECLS

SimDevice::SimDevice()
  : _fd(-1)
{
}

SimDevice::~SimDevice()
{
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(ns)
ELEMENT_PROVIDES(SimDevice)

