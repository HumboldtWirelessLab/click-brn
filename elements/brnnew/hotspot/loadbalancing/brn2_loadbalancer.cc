#include <click/config.h>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>

#include "brn2_loadbalancer.hh"

#include "elements/brn/brnlinkstat.hh"

CLICK_DECLS

LoadBalancer::LoadBalancer():
  _linkstat(NULL),
  _debug(0)
{
}

LoadBalancer::~LoadBalancer()
{
}

int LoadBalancer::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
    cpOptional,
    cpEtherAddress, "ether address", &_me,
    cpKeywords,
    "LINKSTAT", cpElement, "LinkStat", &_linkstat,
    "DEBUG", cpInteger, "debug", &_debug,
    cpEnd) < 0)
      return -1;

  if (!_linkstat || !_linkstat->cast("BRNLinkStat"))
  {
    _linkstat = NULL;
    click_chatter("kein Linkstat");
  }
  return 0;
}

int LoadBalancer::initialize(ErrorHandler *)
{
  return 0;
}

void LoadBalancer::push( int port, Packet *packet )
{
	Vector<EtherAddress> neighbors;
	uint8_t *p_data = (uint8_t *)packet->data();

          if ( _linkstat == NULL )
	{
		output(0).push(packet);
		return;
	}
 
 	_linkstat->get_neighbors(&neighbors);

	if ( neighbors.size() > 0 )
	{
		click_ether *ether = (click_ether *)packet->data();

 		memcpy(&p_data[6] ,neighbors[0].data(), 6);
		memcpy(&p_data[6] , _me.data(), 6 );
		ether->ether_type = 0x8780; /*0*/
 		packet->set_ether_header(ether);
 
		output(1).push(packet);
	}
	else
	{
		output(0).push(packet);
		return;
	}
}

void LoadBalancer::nodeDetection()
{
  if ( _linkstat == NULL ) return;

  _linkstat->get_neighbors(&_neighbors);
}

void LoadBalancer::add_handlers()
{
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LoadBalancer)
