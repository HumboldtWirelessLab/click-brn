#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/straccum.hh>

#include "elements/brn2/standard/brnlogger/brnlogger.hh"
#include "elements/brn2/standard/md5.h"

#include "ip_rangequery.hh"

CLICK_DECLS

IPRangeQuery::IPRangeQuery(IPAddress net, IPAddress mask)
{
  _net = net;
  _mask = mask;
}

IPAddress
IPRangeQuery::get_value_for_id(uint8_t *val)
{
  int prefixlen = _net.mask_to_prefix_len();

  uint32_t intid = ((((( (uint32_t)val[0] << 8 ) + (uint32_t)val[1]) << 8 ) + (uint32_t)val[2]) << 8 ) + (uint32_t)val[3];
  intid >>= prefixlen;

  return ( IPAddress(_net + intid));
}

bool
IPRangeQuery::get_id_for_value(IPAddress ip, uint8_t *val)
{
  if ( ip.matches_prefix(_net, _mask) ) {
    uint32_t intid = ip - _net;
    intid <<= ( 32 - _net.mask_to_prefix_len());

    memset(val,0,MD5_DIGEST_LENGTH);
    val[0] = (uint8_t)((intid >> 24) & 0xFF);
    val[1] = (uint8_t)((intid >> 16) & 0xFF);
    val[3] = (uint8_t)((intid >> 8) & 0xFF);
    val[4] = (uint8_t)((intid) & 0xFF);

    return true;
  }

  return false;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(IPRangeQuery)
