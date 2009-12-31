#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "dhtnode.hh"

#include "elements/brn2/standard/md5.h"

CLICK_DECLS

DHTnode::DHTnode(EtherAddress addr)
{
  _ether_addr = addr;
  _status = STATUS_UNKNOWN;
  _extra = NULL;
  _age = Timestamp::now();
  _rtt = 0;
  _hop_distance = 0;

  MD5::calculate_md5((const char*)MD5::convert_ether2hex(addr.data()).c_str(),
    strlen((const char*)MD5::convert_ether2hex(addr.data()).c_str()), _md5_digest );
}

DHTnode::DHTnode(EtherAddress addr, md5_byte_t *nodeid)
{
  _ether_addr = addr;
  _status = STATUS_UNKNOWN;
  _extra = NULL;
  _age = Timestamp::now();
  memcpy(_md5_digest, nodeid, 16);
}

void
DHTnode::set_age_s(int s)
{
  Timestamp now;
  now = Timestamp::now();

  _age = now - Timestamp(s);
}

int
DHTnode::get_age_s()
{
  Timestamp now;
  now = Timestamp::now();

  return( (now - _age).sec());
}

void
DHTnode::set_age(Timestamp *age)
{
  _age = *age;
}

void
DHTnode::set_age_now()
{
  _age = Timestamp::now();
}

void
DHTnode::set_last_ping_s(int s)
{
  Timestamp now;
  now = Timestamp::now();

  _last_ping = now - Timestamp(s);
}

int
DHTnode::get_last_ping_s()
{
  Timestamp now;
  now = Timestamp::now();

  return( (now - _last_ping).sec());
}

void
DHTnode::set_last_ping(Timestamp *last_ping)
{
  _age = *last_ping;
}

void
DHTnode::set_last_ping_now()
{
  _last_ping = Timestamp::now();
}

String
DHTnode::get_status_string()
{
  switch (_status)
  {
    case STATUS_NEW: return "New";
    case STATUS_OK: return "OK";
    case STATUS_MISSED: return "Missed";
    case STATUS_AWAY: return "Away";
  }
  return "Unknown";
}


CLICK_ENDDECLS

ELEMENT_PROVIDES(DHTnode)
