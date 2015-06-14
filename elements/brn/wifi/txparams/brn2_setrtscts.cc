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
  _rts_cts_mixed_strategy(RTS_CTS_MIXED_STRATEGY_NONE),
  _rts_cts_strategy(RTS_CTS_STRATEGY_ALWAYS_OFF),
  _header(HEADER_WIFI),
  pkt_total(0),
  rts_on(0)
{
  memset(&nstats_dummy,0,sizeof(struct rtscts_neighbour_statistics));
  _scheme_list = SchemeList(String("RtsCtsScheme"));
}

Brn2_SetRTSCTS::~Brn2_SetRTSCTS()
{
}

int Brn2_SetRTSCTS::configure(Vector<String> &conf, ErrorHandler* errh) 
{
  String scheme_string;

  if (cp_va_kparse(conf, this, errh,
    "RTSCTS_SCHEMES", cpkP+cpkM, cpString, &scheme_string,
    "STRATEGY", cpkP, cpInteger, &_rts_cts_strategy,
    "MIXEDSTRATEGY", cpkP, cpInteger, &_rts_cts_mixed_strategy,
    "HEADER", cpkP, cpInteger, &_header,
    "DEBUG", cpkP, cpInteger, &_debug,
        cpEnd) < 0) return -1;

  _scheme_list.set_scheme_string(scheme_string);

  return 0;
}

int Brn2_SetRTSCTS::initialize(ErrorHandler *errh)
{
  BRN_DEBUG("Init Scheme: %d",_rts_cts_strategy);

  click_brn_srandom();

  reset();

  _scheme_list.parse_schemes(this, errh);

  if (_rts_cts_strategy > RTS_CTS_STRATEGY_ALWAYS_ON) {
    _scheme = get_rtscts_scheme(_rts_cts_strategy);
    if (_scheme) _scheme->set_strategy(_rts_cts_strategy);
    else {
      _rts_cts_strategy = RTS_CTS_STRATEGY_NONE;
      BRN_WARN("Scheme(%d) is NULL. Use strategy NONE.",_rts_cts_strategy);
    }
  } else {
    _scheme = NULL;
  }

  if ( _rts_cts_mixed_strategy != RTS_CTS_MIXED_STRATEGY_NONE ) {
    if ( (get_rtscts_scheme(RTS_CTS_STRATEGY_SIZE_LIMIT) == NULL) || (get_rtscts_scheme(RTS_CTS_STRATEGY_HIDDENNODE) == NULL) ||
         ((_rts_cts_mixed_strategy == RTS_CTS_MIXED_PS_AND_HN_AND_RANDOM) && (get_rtscts_scheme(RTS_CTS_STRATEGY_RANDOM) == NULL))) {
      BRN_WARN("Mixed RTS/CTS-Scheme requieres PacketSize-, HiddenNode- and (if random is used) the Random-Scheme");
      _rts_cts_mixed_strategy = RTS_CTS_MIXED_STRATEGY_NONE;
    }
  }

  return 0;
}

RtsCtsScheme *
Brn2_SetRTSCTS::get_rtscts_scheme(uint32_t rts_cts_strategy)
{
  return (RtsCtsScheme *)_scheme_list.get_scheme(rts_cts_strategy);
}

Packet *
Brn2_SetRTSCTS::simple_action(Packet *p)
{
  if (p) {
    bool set_rtscts = false;
    switch (_header) {
      case HEADER_ETHER:
        _pinfo._dst = EtherAddress(((click_ether*)p->data())->ether_dhost);
        break;
      case HEADER_ANNO:
        _pinfo._dst = EtherAddress(BRNPacketAnno::dst_ether_anno(p));
        break;
      case HEADER_AUTO:
      case HEADER_WIFI:
      default:
        _pinfo._dst = EtherAddress(((struct click_wifi *) p->data())->i_addr1);
        break;
    }

    _pinfo._p_size = p->length();
    _pinfo._ceh = WIFI_EXTRA_ANNO(p);
    _pinfo._ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;

    if ( _pinfo._dst.is_broadcast() ) {
      _bcast_nstats->pkt_total++;
      return p;
    } else if ( _rts_cts_mixed_strategy != RTS_CTS_MIXED_STRATEGY_NONE ) {
      switch ( _rts_cts_mixed_strategy ) {
        case RTS_CTS_MIXED_PS_AND_HN:
          set_rtscts = get_rtscts_scheme(RTS_CTS_STRATEGY_SIZE_LIMIT)->set_rtscts(&_pinfo) &&
                       get_rtscts_scheme(RTS_CTS_STRATEGY_HIDDENNODE)->set_rtscts(&_pinfo);
          break;
        case RTS_CTS_MIXED_PS_AND_HN_AND_RANDOM:
          set_rtscts = get_rtscts_scheme(RTS_CTS_STRATEGY_SIZE_LIMIT)->set_rtscts(&_pinfo) &&
                       ( get_rtscts_scheme(RTS_CTS_STRATEGY_HIDDENNODE)->set_rtscts(&_pinfo) ||
                         get_rtscts_scheme(RTS_CTS_STRATEGY_RANDOM)->set_rtscts(&_pinfo));
          break;
        case RTS_CTS_MIXED_PS_AND_FLOODING:
          set_rtscts = get_rtscts_scheme(RTS_CTS_STRATEGY_SIZE_LIMIT)->set_rtscts(&_pinfo) &&
                       get_rtscts_scheme(RTS_CTS_STRATEGY_FLOODING)->set_rtscts(&_pinfo);
          break;
        default:
          BRN_WARN("Unknown CombinedRTSCTS Scheme!");
          set_rtscts = false;
          break;
      }
    } else {
      switch ( _rts_cts_strategy ) {
        case RTS_CTS_STRATEGY_NONE:
        case RTS_CTS_STRATEGY_ALWAYS_OFF:
          break;
        case RTS_CTS_STRATEGY_ALWAYS_ON:
          set_rtscts = true;
          break;
        default:
          set_rtscts = _scheme->set_rtscts(&_pinfo);
          break;
      }
    }

    _pinfo._ceh->magic = WIFI_EXTRA_MAGIC;

    pkt_total++;

    struct rtscts_neighbour_statistics *nstats = rtscts_neighbours.findp(_pinfo._dst);
    if (NULL == nstats) {
      rtscts_neighbours.insert(_pinfo._dst, nstats_dummy);
      nstats = rtscts_neighbours.findp(_pinfo._dst);
    }

    nstats->pkt_total++;

    if (set_rtscts) {
      _pinfo._ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;

      rts_on++;
      nstats->rts_on++;
    }
  }

  return p;
}


String
Brn2_SetRTSCTS::stats()
{
  StringAccum sa;
  sa << "<setrtscts node=\""<< BRN_NODE_NAME << "\" strategy=\"" << _rts_cts_strategy << "\" mixed_strategy=\"" << _rts_cts_mixed_strategy << "\" >\n";

  sa << "\t<schemes>\n";
  sa << "\t\t<scheme name=\"RtsCtsNone\" id=\"" << (int)RTS_CTS_STRATEGY_NONE << "\" active=\"";
  sa << (int)((_rts_cts_strategy==RTS_CTS_STRATEGY_NONE)?1:0) << "\" />\n";
  sa << "\t\t<scheme name=\"RtsCtsAllwaysOff\" id=\"" << (int)RTS_CTS_STRATEGY_ALWAYS_OFF << "\" active=\"";
  sa << (int)((_rts_cts_strategy==RTS_CTS_STRATEGY_ALWAYS_OFF)?1:0) << "\" />\n";
  sa << "\t\t<scheme name=\"RtsCtsAllwaysOn\" id=\"" << (int)RTS_CTS_STRATEGY_ALWAYS_ON << "\" active=\"";
  sa << (int)((_rts_cts_strategy==RTS_CTS_STRATEGY_ALWAYS_ON)?1:0) << "\" />\n";
  for (uint16_t i = 0; i <= _scheme_list._max_scheme_id; i++) {
    Element *e = (Element *)_scheme_list.get_scheme(i);
    if ( e == NULL ) continue;
    sa << "\t\t<scheme name=\"" << e->class_name() << "\" id=\"" << i;
    sa << "\" active=\"" << (int)(((Scheme *)e->cast("Scheme"))->handle_strategy(_rts_cts_strategy)?1:0) << "\" />\n";
  }
  sa << "\t</schemes>\n";

  sa << "\t<neighbours>\n";
  for (HashMap<EtherAddress,struct rtscts_neighbour_statistics>::const_iterator it = rtscts_neighbours.begin(); it.live(); ++it) {
    EtherAddress ea = it.key();
    struct rtscts_neighbour_statistics* nstats = rtscts_neighbours.findp(ea);

    sa << "\t\t<neighbour address=\"" << ea.unparse().c_str() << "\" packets_total=\"" << (int)(nstats->pkt_total);
    sa << "\" rts_on=\"" << (int)(nstats->rts_on) << "\" rts_off=\"" << (int)(nstats->pkt_total - nstats->rts_on) <<"\"/>\n";
  }

  sa.append("\t</neighbours>\n</setrtscts>\n");

  return sa.take_string();
}

void Brn2_SetRTSCTS::reset()
{
  rtscts_neighbours.clear();
  rtscts_neighbours.insert(ETHERADDRESS_BROADCAST, nstats_dummy);
  _bcast_nstats = rtscts_neighbours.findp(ETHERADDRESS_BROADCAST);
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
        if (IntArg().parse(s, strategy)) f->set_strategy(strategy);
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
