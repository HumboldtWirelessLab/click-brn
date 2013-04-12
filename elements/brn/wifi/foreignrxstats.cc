#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brn2.h"

#include "foreignrxstats.hh"

CLICK_DECLS

ForeignRxStats::ForeignRxStats():
  last_packet(NULL),
  fwd_timer(this),
  ack_timeout(FOREIGNRXSTATS_DEFAULT_ACK_TIMEOUT)
{
  BRNElement::init();
}

ForeignRxStats::~ForeignRxStats()
{
}

int
ForeignRxStats::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  
  ret = cp_va_kparse(conf, this, errh,
		     "DEVICE", cpkP+cpkM, cpElement, &_device,
		     "TIMEOUT", cpkP, cpInteger, &ack_timeout,
                     "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd);

  return ret;

}

int
ForeignRxStats::initialize(ErrorHandler *)
{
  fwd_timer.initialize(this);
  return 0;
}
  
void
ForeignRxStats::run_timer(Timer *)
{
  send_outstanding_packet(true); 
}
  
Packet *
ForeignRxStats::simple_action(Packet *p)
{

  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  
  if ( ceh->flags & WIFI_EXTRA_TX ) {
    send_outstanding_packet(true);
    return p;
  } else { 
 
    struct click_wifi *wh = (struct click_wifi *) p->data();

    EtherAddress dst = EtherAddress(wh->i_addr1);

    if ( dst.is_broadcast() || (dst == *_device->getEtherAddress()) ) {
      send_outstanding_packet(true);
      return p;
    }
    
    int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
    
    //Don't handle managment frames
    if ( type == WIFI_FC0_TYPE_MGT ) {
      send_outstanding_packet(true);
      return p;
    }
  
    if ((type ==  WIFI_FC0_TYPE_CTL) && ((wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) == WIFI_FC0_SUBTYPE_ACK) && (last_packet != NULL)) {
      BRN_DEBUG("Ack: %d",(p->timestamp_anno() - last_packet->timestamp_anno()).msecval() );
      BRN_DEBUG("Last: %d (%s, %s)  P: %d (%s, %s) Timeout: %d",last_packet->length(), last_packet->timestamp_anno().unparse().c_str(), last_packet_src.unparse().c_str(),
		                                                p->length(), p->timestamp_anno().unparse().c_str(), dst.unparse().c_str(), ack_timeout);
      
      if ((last_packet != NULL) && (p->length() >= 10) && (last_packet_src == dst) &&
	  ((p->timestamp_anno() - last_packet->timestamp_anno()).msecval() <= ack_timeout)) {
	BRN_ERROR("Data was acked");
        send_outstanding_packet(false);    
      } else {
	BRN_ERROR("Ack but no data");
        send_outstanding_packet(true);
      }
      
      return p;
      
    } else if (type ==  WIFI_FC0_TYPE_DATA) {
      last_packet_src = EtherAddress(wh->i_addr2);
      send_outstanding_packet(true);
      last_packet = p;      
        
      fwd_timer.reschedule_after_msec(ack_timeout);

    } else {
      send_outstanding_packet(true);
      return p;    
    }       
  }

  return NULL;
}

void
ForeignRxStats::send_outstanding_packet(bool failed)
{
  if ( last_packet != NULL ) {
    if ( ! failed ) {
     struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(last_packet);
     ceh->flags |= WIFI_EXTRA_FOREIGN_TX_SUCC;
    }
    output(0).push(last_packet);
    last_packet = NULL;
  }
}

enum {H_STATS};

String
ForeignRxStats::stats_handler()
{
  StringAccum sa;

  sa << "<collisioninfo node=\"" << BRN_NODE_NAME << "\" >\n";
/*  for (RetryStatsTableIter iter = rs_tab.begin(); iter.live(); iter++) {
    RetryStats *rs = iter.value();
    EtherAddress ea = iter.key();

    sa << "\t<nb addr=\"" << ea.unparse() << "\" time=\"" << rs->_last_index_inc.unparse() << "\" >\n";

    for ( int i = 0; i < rs->_no_queues; i++ ) {
      sa << "\t\t<queue no=\"" << i << "\" succ_rate=\"";

      if (rs->_l_unicast_tx[i] == 0) {
        sa << "-1";
      } else {
        sa << ( (100 * rs->_l_unicast_succ[i]) / rs->_l_unicast_tx[i] );
      }
      sa << "\" />\n";
    }

    sa << "\t</nb>\n";
  }

  sa << "</collisioninfo>\n";
*/
  return sa.take_string();
}

static String
ForeignRxStats_read_param(Element *e, void *thunk)
{
  ForeignRxStats *td = (ForeignRxStats *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->stats_handler();
      break;
  }

  return String();
}

void
ForeignRxStats::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", ForeignRxStats_read_param, (void *) H_STATS);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(ForeignRxStats)
