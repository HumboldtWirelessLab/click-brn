#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/straccum.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/standard/brn_md5.hh"

#include "ip_rangequery.hh"

CLICK_DECLS

IPRangeQuery::IPRangeQuery(IPAddress net, IPAddress mask): _net(net), _mask(mask)
{
}

IPAddress
IPRangeQuery::get_value_for_id(uint8_t *val)
{
  int prefixlen = _mask.mask_to_prefix_len();

  /*char buf[128];
  click_chatter("NET: %s Prefixlen: %d",_net.unparse().c_str(),prefixlen);
  MD5::printDigest(val, buf);
  click_chatter("Dig: %s",buf);*/

  uint32_t intid = ((((( (uint32_t)val[0] << 8 ) + (uint32_t)val[1]) << 8 ) + (uint32_t)val[2]) << 8 ) + (uint32_t)val[3];
  intid >>= prefixlen;

//click_chatter("NET: %s",IPAddress(htonl(ntohl(_net) + intid)).unparse().c_str());

  return IPAddress(htonl(ntohl(_net) + intid));
}

bool
IPRangeQuery::get_id_for_value(IPAddress ip, uint8_t *val)
{
  if ( ip.matches_prefix(_net, _mask) ) {
    uint32_t intid = ntohl(ip.addr()) - ntohl(_net.addr());
//    click_chatter("ID->key: IP: %d Net: %d  ident: %d",ip.addr(),_net.addr(), intid);
    intid <<= _mask.mask_to_prefix_len();

    memset(val,0,MD5_DIGEST_LENGTH);
    val[0] = (uint8_t)((intid >> 24) & 0xFF);
    val[1] = (uint8_t)((intid >> 16) & 0xFF);
    val[3] = (uint8_t)((intid >> 8) & 0xFF);
    val[4] = (uint8_t)((intid) & 0xFF);

/*    char buf[128];
    int prefixlen = _mask.mask_to_prefix_len();
    click_chatter("IP->ID NET: %s Prefixlen: %d",_net.unparse().c_str(),prefixlen);
    MD5::printDigest(val, buf);
    click_chatter("IP->ID IP: %s DIGEST: %s",ip.unparse().c_str(), buf);*/

    return true;
  }

  return false;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(IPRangeQuery)
