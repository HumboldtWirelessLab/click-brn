/*
 *  
 */

#include <click/config.h>//have to be always the first include
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <clicknet/wifi.h>

#include "elements/brn/brn2.h"
#include "elements/brn/brnprotocol/brnpacketanno.hh"

#include "brn2_setrtscts.hh"



CLICK_DECLS

Brn2_SetRTSCTS::Brn2_SetRTSCTS():
  _scheme(NULL),
  _rts_cts_strategy(RTS_CTS_STRATEGY_ALWAYS_OFF),
  pkt_total(0),
  rts_on(0)
{
  memset(&nstats_dummy,0,sizeof(struct rtscts_neighbour_statistics));
}

Brn2_SetRTSCTS::~Brn2_SetRTSCTS() {
}

int Brn2_SetRTSCTS::initialize(ErrorHandler *) {
  click_brn_srandom();

  rtscts_neighbours.insert(ETHERADDRESS_BROADCAST, nstats_dummy);

  return 0;
}

int Brn2_SetRTSCTS::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  if (cp_va_kparse(conf, this, errh,
    "STRATEGY", cpkP, cpInteger, &_rts_cts_strategy,
    "RTSCTS_SCHEMES", cpkP, cpElement, &_scheme,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;
  return 0;
}

int
Brn2_SetRTSCTS::parse_schemes(String s_schemes, ErrorHandler* errh)
{
  Vector<String> schemes;

  String s_schemes_uncomment = cp_uncomment(s_schemes);
  cp_spacevec(s_schemes_uncomment, schemes);

  _no_schemes = schemes.size();

  if (_no_schemes == 0) {
    BRN_DEBUG("parse_schemes(): No schemes were given! STRAT = %d\n", _rts_cts_strategy);
    _scheme = NULL;
    return 0;
  }

  for (uint16_t i = 0; i < _no_schemes; i++) {
    Element *e = cp_element(schemes[i], this, errh);
    BackoffScheme *bo_scheme = (BackoffScheme *)e->cast("");

    if (!bo_scheme) {
      return errh->error("Element is not a 'BackoffScheme'");
    } else {
      _bo_schemes[i] = bo_scheme;
      _bo_schemes[i]->set_conf(_cwmin[0], _cwmin[no_queues - 1]);
    }
  }

  BRN_DEBUG("Tos2QM.parse_bo_schemes(): strat %d no_schemes %d\n", _bqs_strategy, _no_schemes);

  _current_scheme = get_bo_scheme(_bqs_strategy); // get responsible scheme

  if ( _current_scheme != NULL ) {
    _current_scheme->set_strategy(_bqs_strategy); // set final strategy on that scheme
  }

  return 0;
}


Packet *Brn2_SetRTSCTS::simple_action(Packet *p)
{
  if (p) {
    bool set_rtscts = false;
    EtherAddress dst = EtherAddress(BRNPacketAnno::dst_ether_anno(p));

    switch ( _rts_cts_strategy ) {
      case RTS_CTS_STRATEGY_ALWAYS_OFF:
        break;
      case RTS_CTS_STRATEGY_ALWAYS_ON:
        set_rtscts = true;
        break;
      default:
        set_rtscts = _scheme->set_rtscts(dst, p->length());
        break;
    }

    struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
    ceh->magic = WIFI_EXTRA_MAGIC;

    pkt_total++;

    struct rtscts_neighbour_statistics *nstats = rtscts_neighbours.findp(dst);
    if (NULL == nstats) {
      rtscts_neighbours.insert(dst, nstats_dummy);
      nstats = rtscts_neighbours.findp(dst);
    }

    nstats->pkt_total++;

    if (set_rtscts) {
      ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;

      rts_on++;
      nstats->rts_on++;
    } else {
      ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;
    }
  }

  return p;
}


String
Brn2_SetRTSCTS::stats()
{
  StringAccum sa;
  sa << "<setrtscts node=\""<< BRN_NODE_NAME << "\" strategy=\"" << _rts_cts_strategy << "\" >\n";

  for (HashMap<EtherAddress,struct rtscts_neighbour_statistics>::const_iterator it = rtscts_neighbours.begin(); it.live(); ++it) {
    EtherAddress ea = it.key();
    struct rtscts_neighbour_statistics* nstats = rtscts_neighbours.findp(ea);

    sa << "\t<neighbour address=\"" << ea.unparse().c_str() <<"\" packets_total=\"" << nstats->pkt_total;
    sa << "\" rts_on=\"" << nstats->rts_on << "\" rts_off=\"" << (nstats->pkt_total - nstats->rts_on) <<"\"/>\n";
  }

  sa.append("</setrtscts>\n");

  return sa.take_string();
}

void Brn2_SetRTSCTS::reset()
{
  rtscts_neighbours.clear();
  EtherAddress broadcast_address = broadcast_address.make_broadcast();
  rtscts_neighbours.insert(broadcast_address,nstats_dummy); 
}

enum {H_RTSCTS_STATS, H_RTSCTS_RESET, H_RTSCTS_STRATEGY};

static String SetRTSCTS_read_param(Element *e, void *thunk)
{
  Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
  switch ((uintptr_t) thunk) {
    case H_RTSCTS_STATS:
      return  f->stats(); 
     case H_RTSCTS_STRATEGY:
        return(String(f->get_strategy()));
    default:
      return String();
  }
}


static int SetRTSCTS_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler * /*errh*/)
{
  Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
  String s = cp_uncomment(in_s);

  switch ((intptr_t)vparam) {
     case H_RTSCTS_RESET:
        f->reset();
        break;
     case H_RTSCTS_STRATEGY:
        unsigned strategy;
        if (!IntArg().parse(s, strategy)) f->set_strategy(strategy);
        break;
  }

  return 0;
}


void Brn2_SetRTSCTS::add_handlers()
{
  BRNElement::add_handlers();//for Debug-Handlers

  add_read_handler("stats",SetRTSCTS_read_param,H_RTSCTS_STATS);
  add_read_handler("strategy", SetRTSCTS_read_param,H_RTSCTS_STRATEGY);

  add_write_handler("reset",SetRTSCTS_write_param,H_RTSCTS_RESET, Handler::h_button);//see include/click/handler.hh 
  add_write_handler("strategy", SetRTSCTS_write_param,H_RTSCTS_STRATEGY);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2_SetRTSCTS)
