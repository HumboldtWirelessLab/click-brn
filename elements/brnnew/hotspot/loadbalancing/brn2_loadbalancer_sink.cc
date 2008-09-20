#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "brn2_loadbalancer_sink.hh"

CLICK_DECLS

LoadBalancerSink::LoadBalancerSink():
  _debug(0)
{
}

LoadBalancerSink::~LoadBalancerSink()
{
}

int LoadBalancerSink::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpEtherAddress, "ether address", &_me,
    cpKeywords,
    "DEBUG", cpInteger, "debug", &_debug,
    cpEnd) < 0)
      return -1;

  return 0;
}

int LoadBalancerSink::initialize(ErrorHandler *)
{
  return 0;
}

void LoadBalancerSink::push( int port, Packet *packet )
{
	if ( port == 0 )
	{
		click_chatter("Paket from Net! push back to src");

	          /*click_ether *ether = (click_ether *)packet->data();
		uint8_t *p_data = (uint8_t *)packet->data();

 		memcpy(&p_data[6] ,neighbors[0].data(), 6);
		memcpy(&p_data[6] , _me.data(), 6 );
		ether->ether_type = 0x8780; /*0*/
 	/*	packet->set_ether_header(ether);
*/
		output(1).push(packet);
	}
	else
	{
		click_chatter("Paket from Neighbor! Try to push it to Internet");
 
		output(0).push(packet);
	}

	return;
}

void LoadBalancerSink::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LoadBalancerSink)

