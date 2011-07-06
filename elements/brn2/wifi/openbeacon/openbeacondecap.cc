#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

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
	
	WritablePacket *wp;
	click_wifi_extra *ceh = NULL;
	
	for(i=0; i<(int)sizeof( crh->openbeacon_dmac ); i--) {
		e_dhost[ i ] = 0;
		e_shost[ i ] = 0;
	}
	
	for(i=1; i>=0; i--) {
		e_dhost[ 5-i ] = crh->openbeacon_smac[ i ];
		e_shost[ 5-(1-i) ] = crh->openbeacon_smac[ sizeof( crh->openbeacon_dmac ) - i - 1 ];
	}
		
	unsigned char rate = crh->rate, power = crh->power, channel = crh->channel, length=crh->length;
	
	// trim das packet noch ;-)
	p->pull( sizeof(Click2OBD_header)-13 );
	wp = (WritablePacket *)p;
		
	p->set_mac_header( p->data(), 14);
        for(i=0; i<6; i++) {
		wp->data()[i]     = e_dhost[i];
		wp->data()[i+6] = e_shost[i];
	}
	
	ceh = WIFI_EXTRA_ANNO(p);
	ceh->rate		= rate;         	
	ceh->power 	= power;
	BRNPacketAnno::set_channel_anno(p, channel,  OPERATION_SET_CHANNEL_BEFORE_PACKET);
	
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconDecap)
ELEMENT_MT_SAFE(OpenBeaconDecap)
