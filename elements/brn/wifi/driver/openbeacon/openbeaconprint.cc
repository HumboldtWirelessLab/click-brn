#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include <elements/brn/brnprotocol/brnpacketanno.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "openbeaconprint.hh"
#include "openbeacon_comunication.h"

CLICK_DECLS

OpenBeaconPrint::OpenBeaconPrint()
  :_debug(BrnLogger::DEFAULT)
{
	
}

OpenBeaconPrint::~OpenBeaconPrint()
{
}

int
OpenBeaconPrint::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
	
  ret = cp_va_kparse(conf, this, errh,
		     "LABEL", cpkP, cpString, &_label,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
  return ret;
}

Packet *
OpenBeaconPrint::simple_action(Packet *p)
{

	Click2OBD_header *crh = (Click2OBD_header *)p->data();
	StringAccum dmac_sa( sizeof(crh->openbeacon_dmac)*4 ), smac_sa( sizeof(crh->openbeacon_smac)*4);
	char cpower[10], cchannel[10], crate[10], clength[10];
	
	for(int i=0; i<sizeof(crh->openbeacon_dmac); i++)  {
		unsigned int dlen = sprintf(dmac_sa.reserve( 4 ), "%.2X:", crh->openbeacon_dmac[i]);
		unsigned int slen = sprintf(smac_sa.reserve( 4 ), "%.2X:", crh->openbeacon_smac[i]);
			
		if(i<sizeof(crh->openbeacon_dmac)-1) dmac_sa.adjust_length( dlen );	else dmac_sa.adjust_length( dlen-1 );
		if(i<sizeof(crh->openbeacon_dmac)-1) smac_sa.adjust_length( slen ); 	else smac_sa.adjust_length( slen-1 );
	}
	if(crh->power>0) sprintf(cpower, "%d", crh->power-1); else sprintf(cpower, "current");
	if(crh->channel>0) sprintf(cchannel, "%d", crh->channel-1); else sprintf(cchannel, "current");
	if(crh->rate>0) sprintf(crate, "%d", crh->rate-1); else sprintf(crate, "current");
	sprintf(clength, "%d", (int)(crh->length));
	
	click_chatter("%s# Size: %s, Power: %s, Rate: %s, Channel: %s, DEST: %s, SRC: %s",_label.c_str(), clength, cpower, crate, cchannel, dmac_sa.c_str(), smac_sa.c_str());
	return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(OpenBeaconPrint)
ELEMENT_MT_SAFE(OpenBeaconPrint)
