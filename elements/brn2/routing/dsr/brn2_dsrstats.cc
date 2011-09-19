#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <clicknet/ether.h>

#include "brn2_dsrencap.hh"
#include "brn2_dsrdecap.hh"

#include "brn2_dsrstats.hh"

CLICK_DECLS

DSRStats::DSRStats()
{
  BRNElement::init();
}

DSRStats::~DSRStats()
{
}

int DSRStats::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "DEBUG", cpkN, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int DSRStats::initialize(ErrorHandler *)
{
  return 0;
}

void
DSRStats::push( int port, Packet *packet )
{
  if ( ( port == 0 ) && ( packet != NULL ) ) {
    click_brn_dsr *brn_dsr = (click_brn_dsr *)(packet->data() + sizeof(click_brn));

    int hop_count = brn_dsr->dsr_hop_count;

    click_dsr_hop *dsr_hops = DSRProtocol::get_hops(brn_dsr);

    EtherAddress src(brn_dsr->dsr_src.data);
    EtherAddress dst(brn_dsr->dsr_dst.data);

    BRN_DEBUG("Found DSR-Header: Src %s -> Dst %s",src.unparse().c_str(),dst.unparse().c_str());

    RouteEntry *re = route_map.find(EtherAddressPair(src,dst));

    if ( re == NULL ) {
      re = new RouteEntry(src,dst);
      route_map.insert(EtherAddressPair(src,dst),re);
    }

    int r = 0;
    for (; r < re->routes.size(); r++) {
      RouteInfo *ri = re->routes[r];

      if ( same_route(ri, dsr_hops, hop_count) ) break;
    }

    if ( r == re->routes.size() ) {
      re->insert(dsr_hops, hop_count, ntohs(brn_dsr->dsr_id));
    } else {
      re->routes[r]->inc_usage();
    }

    output(0).push(packet);
  }
}

bool
DSRStats::same_route(RouteInfo *ri, click_dsr_hop *dsr_hops, int hop_count)
{
  if ( hop_count != ri->route.size() ) return false;

  for ( int h = 0; h < ri->route.size(); h++ )
    if ( memcmp(ri->route[h].data(), dsr_hops[h].hw.data, 6) != 0 ) return false;

  return true;
}

enum {H_STATS, H_RESET};

String
DSRStats::read_stats()
{
  StringAccum sa;

  sa << "<dsr_route_stats id=\"" << BRN_NODE_NAME << "\" node_pairs=\"" << route_map.size() << "\" >\n";

  for (RouteMapIter iter = route_map.begin(); iter.live(); iter++) {
    RouteEntry *re = route_map.find(iter.key());

    sa << "\t<route_info src=\"" << re->src.unparse() << "\" dst=\"" << re->dst.unparse() << "\" >\n";
    for ( int r = 0; r < re->routes.size(); r++) {
      RouteInfo *ri = re->routes[r];

      sa << "\t\t<route id=\"" << ri->id << "\" metric=\"" << ri->metric << "\" usage=\"" << ri->count_usage;
      sa << "\" last_usage=\"" << ri->last_use.unparse() << "\" hop_count=\"" << ri->route.size() + 1;
      sa << "\" hops=\"" << re->src.unparse();

      for ( int h = 0; h < ri->route.size(); h++ )
        sa << "," << ri->route[h].unparse();

      sa << "," << re->dst.unparse() << "\" />\n";
    }
    sa << "\t</route_info>\n";
  }
  sa << "</dsr_route_stats>\n";

  return sa.take_string();
}

void
DSRStats::reset()
{
  for (RouteMapIter iter = route_map.begin(); iter.live(); iter++) {
    RouteEntry *re = route_map.find(iter.key());
    delete re;
  }
  route_map.clear();
}


static String 
DSRStats_read_param(Element *e, void *thunk)
{
  StringAccum sa;
  DSRStats *td = (DSRStats *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS:
      return td->read_stats();
      break;
  }

  return String();
}

static int 
DSRStats_write_param(const String &/*in_s*/, Element *e, void *vparam, ErrorHandler */*errh*/)
{
  DSRStats *f = (DSRStats *)e;

  switch((intptr_t)vparam) {
    case H_RESET: {    //reset
      f->reset();
    }
  }

  return 0;
}

void
DSRStats::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", DSRStats_read_param, (void *) H_STATS);

  add_write_handler("reset", DSRStats_write_param, (void *) H_RESET);

}

CLICK_ENDDECLS
EXPORT_ELEMENT(DSRStats)
