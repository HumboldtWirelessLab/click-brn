#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "brn2_dhtnode.hh"

#include "elements/brn/dht/md5.h"

CLICK_DECLS

DHTnode::DHTnode(EtherAddress addr)
{
  _ether_addr = addr;
  _status = STATUS_UNKNOWN;
  _extra = NULL;
  _age.set_now();
  MD5helper::calculate_md5((const char*)MD5helper::convert_ether2hex(addr.data()).c_str(),
    strlen((const char*)MD5helper::convert_ether2hex(addr.data()).c_str()), _md5_digest );
}

DHTnode::DHTnode(EtherAddress addr, md5_byte_t *nodeid)
{
  _ether_addr = addr;
  _status = STATUS_UNKNOWN;
  _extra = NULL;
  _age.set_now();
  memcpy(_md5_digest, nodeid, 16);
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(DHTnode)
