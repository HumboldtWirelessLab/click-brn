#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/error.hh>
#include <elements/brn2/wifi/brnwifi.hh>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "openbeacontests.hh"
#include "openbeacon_comunication.h"

#include <time.h>

CLICK_DECLS

OpenBeaconTests::OpenBeaconTests()
  :_debug(BrnLogger::DEFAULT)
{
	
}

OpenBeaconTests::~OpenBeaconTests()
{
}

int
OpenBeaconTests::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
	
  _mode = 0;	
  _count = 0;
  ret = cp_va_kparse(conf, this, errh,
		     "MODE", cpkP, cpInteger, &_mode,
		     "COUNT", cpkP, cpInteger, &_count,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);
	
  // check parameter
  if(_mode==0) {
	errh->error("MODE parameter must be an integer value.      value 1: send COUNT packets over hw-link\n\t\t\t\t\t\tvalue 2: send a packets to hw. ");
	errh->error("\n\t\t\t\t\t\tvalue 3: send one packets from click over hw-link to click");
	return -1;
  }	
  if(_mode==1 && _count==0) {
	errh->error("COUNT parameter must be an integer value.     send COUNT packets to hw. ");
	return -1;
  }

  return ret;
}

  void OpenBeaconTests::push(int port, Packet *p)
{
	uint32_t edata_length = 0;
	WritablePacket* wP = p->uniqueify();
	
	if( wP && wP->has_mac_header() )  {
		if(port==0) {
			// Send Testpacket
			edata_length = p->length() - sizeof( struct click_ether );
					
			// set space for data
			if(  (sizeof(HW_rxtx_Test)-OPENBEACON_MACSIZE-sizeof(uint16_t)) > edata_length ) {
				wP =  wP->put( (sizeof(HW_rxtx_Test)-OPENBEACON_MACSIZE-sizeof(uint16_t)) - edata_length);
			} else {
				wP->take( edata_length - (sizeof(HW_rxtx_Test)-OPENBEACON_MACSIZE-sizeof(uint16_t)) );
			}
			if(wP) {
				unsigned char* data = wP->data() + 6 + (6 - OPENBEACON_MACSIZE);
				HW_rxtx_Test *  hwt = (HW_rxtx_Test*) data;

				hwt->type = _mode;
				if(_mode==1) { hwt->count = _count; 
					click_chatter("OpenBeaconTests::push: Mode %d , Count %d", hwt->type, hwt->count);
				} else {
					click_chatter("OpenBeaconTests::push: Mode %d", hwt->type);
				}
				
				hwt->prot_type[0] = 0x06;
				hwt->prot_type[1] = 0x06;  // set valid proto type 

				// TODO: insert the Host-Timestamp 
				
				output(0).push(wP);
			}
		
		}
		if(port==1) {
			// Recive testpacket
			
			// check type
			unsigned char* data = wP->data() + 6 + (6 - OPENBEACON_MACSIZE);
			HW_rxtx_Test *  hwt = (HW_rxtx_Test*) data;
			
			if(hwt->prot_type[0] == 0x06 && hwt->prot_type[1] == 0x06)  {
				if(hwt->type==1) {  
					click_chatter("OpenBeaconTests::push: Test1");
					
				} 
				if(hwt->type==2) {
					click_chatter("OpenBeaconTests::push: Test2");
					
				}
				if(hwt->type==3) {
					click_chatter("OpenBeaconTests::push: Test3");
					
				} 
			}
			
		}	
	}
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OpenBeaconTests)
ELEMENT_MT_SAFE(OpenBeaconTests)

