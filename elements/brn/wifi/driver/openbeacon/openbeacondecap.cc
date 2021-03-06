#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "openbeacondecap.hh"
#include "openbeacon_comunication.h"

CLICK_DECLS

OpenBeaconDecap::OpenBeaconDecap()
  :_debug(BrnLogger::DEFAULT)
{
	
}

OpenBeaconDecap::~OpenBeaconDecap()
{
}

int
OpenBeaconDecap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;

  ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
OpenBeaconDecap::simple_action(Packet *p)
{
	Click2OBD_header *crh = (Click2OBD_header *)p->data(); 
	uint8_t e_dhost[6], e_shost[6];
	int i=0;
	uint8_t ob_bcats[] = {255, 255};
	
	WritablePacket *wp;
	click_wifi_extra *ceh = NULL;

	memcpy( e_dhost, "\0\0\0\0\0\0" ,6 );
	memcpy( e_shost, "\0\0\0\0\0\0" ,6 );

	for(i=1; i>=0; i--) {
		e_dhost[ 5-i ] = crh->openbeacon_smac[ i ];
		e_shost[ 5-(1-i) ] = crh->openbeacon_smac[ sizeof( crh->openbeacon_dmac ) - i - 1 ];
	}

	if ( memcmp(&(e_dhost[4]),ob_bcats,2) == 0 ) {
	  uint8_t ob_bcats_ext[] = {255, 255,255,255};
	  memcpy( e_dhost, ob_bcats_ext ,4 );
	}

	unsigned char rate = crh->rate, power = crh->power, channel = crh->channel/*, length=crh->length*/;
	
	p->pull( sizeof(Click2OBD_header)-12 );
	wp = reinterpret_cast<WritablePacket *>(p);
		
	p->set_mac_header( p->data(), 14);
    for(i=0; i<6; i++) {
		wp->data()[i]     = e_dhost[ i ] ;
		wp->data()[i+6] = e_shost[ i ] ;
	}
	
	ceh = WIFI_EXTRA_ANNO(p);
	ceh->rate	= rate;
	ceh->power 	= power;
	BRNPacketAnno::set_channel_anno(p, channel,  OPERATION_SET_CHANNEL_BEFORE_PACKET);
	
	return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(OpenBeaconDecap)
ELEMENT_MT_SAFE(OpenBeaconDecap)
