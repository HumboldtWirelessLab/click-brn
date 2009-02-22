#include <click/config.h>
#include <click/element.hh>
#include <click/etheraddress.hh>

#include "powerratestats.hh"

CLICK_DECLS

PowerRateStats::DstInfo *
PowerRateStats::newDst(unsigned char *ea)
{
  PowerRateStats::DstInfo *newDst;

  newDst = new DstInfo(ea);
  _dstlist.push_back(newDst);

  return newDst;
}

PowerRateStats::DstInfo *
PowerRateStats::getDst(EtherAddress *ea)
{
  if ( ea != NULL ) return getDst(ea->data());
  return NULL;
}

PowerRateStats::DstInfo *
PowerRateStats::getDst(EtherAddress ea)
{
  return getDst(ea.data());
}

PowerRateStats::DstInfo *
PowerRateStats::getDst(unsigned char *ea)
{
  for ( int i = 0; i < _dstlist.size(); i++)
    if ( memcmp(_dstlist[i]->_dst.data(),ea,6) == 0 ) return _dstlist[i];

  return NULL;
}


CLICK_ENDDECLS

ELEMENT_PROVIDES(PowerRateStats)
