#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "rxpowerstats.hh"

CLICK_DECLS

RXPowerStats::DstInfo *
RXPowerStats::newDst(unsigned char *ea)
{
  RXPowerStats::DstInfo *newDst;

  newDst = new DstInfo(ea);
  _dstlist.push_back(newDst);

  return newDst;
}

RXPowerStats::DstInfo *
RXPowerStats::getDst(EtherAddress *ea)
{
  if ( ea != NULL ) return getDst(ea->data());
  return NULL;
}

RXPowerStats::DstInfo *
RXPowerStats::getDst(EtherAddress ea)
{
  return getDst(ea.data());
}

RXPowerStats::DstInfo *
RXPowerStats::getDst(unsigned char *ea)
{
  for ( int i = 0; i < _dstlist.size(); i++)
    if ( memcmp(_dstlist[i]->_dst.data(),ea,6) == 0 ) return _dstlist[i];

  return NULL;
}


CLICK_ENDDECLS

ELEMENT_PROVIDES(RXPowerStats)
