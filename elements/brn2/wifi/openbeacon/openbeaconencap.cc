#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
//#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "openbeaconencap.hh"
#include "openbeacon_comunication.h"

CLICK_DECLS

OpenBeaconEncap::OpenBeaconEncap()
  :_debug(BrnLogger::DEFAULT)
{
	
}

OpenBeaconEncap::~OpenBeaconEncap()
{
}

int
OpenBeaconEncap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
		     "SRC", cpkP+cpkM, cpEthernetAddress, &opbecon_filter,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  _errh = errh;
  return ret;
}

Packet *
OpenBeaconEncap::simple_action(Packet *p)
{
  WritablePacket *q;
  Click2OBD_header *crh = NULL; 
  click_wifi_extra *ceh = NULL;
	
  // save MAC
  uint8_t	e_dhost[6], e_shost[6];
  int i=0;	
  
  for(i=0; i<6; i++) { 
	e_dhost[i] = p->ether_header()->ether_dhost[i];
	e_shost[i] = p->ether_header()->ether_shost[i];
  }
  
  q = p->push( sizeof(Click2OBD_header)-12  );  

  if ( !q ) {
    p->kill();
    return NULL;
  }
  
  ceh = WIFI_EXTRA_ANNO(q);
  crh = (Click2OBD_header *)q->data();
  
  crh->length	= q->length()-sizeof(Click2OBD_header);
  crh->status    = 0;
  crh->channel	= BRNPacketAnno::channel_anno(q);
  crh->power	= ceh->power;

  if ( ceh->rate == 0 ) {
	crh->rate = 0;
  } else {
	crh->rate = (ceh->rate/2);
  }
  
  for(i=1; i>=0; i--) {
	  crh->openbeacon_smac[ i ] =   e_dhost[ 5-i ];  // ok
	  crh->openbeacon_smac[ sizeof( crh->openbeacon_dmac ) - i - 1 ] =  e_shost[ 5-(1-i) ];
  }
  for(i=sizeof( crh->openbeacon_dmac ); i>0; i--) {
	  crh->openbeacon_dmac[ 6 - i - 1 ] =  opbecon_filter[ sizeof( crh->openbeacon_dmac ) - i + 1];
  }  
  
  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconEncap)
ELEMENT_MT_SAFE(OpenBeaconEncap)
