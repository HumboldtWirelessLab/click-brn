#ifndef BRN_DSR_STATS_HH
#define BRN_DSR_STATS_HH
#include <click/element.hh>
#include <click/hashmap.hh>
#include <click/pair.hh>
#include <click/vector.hh>

#include <click/etheraddress.hh>

#include "elements/brn/brnelement.hh"

CLICK_DECLS

class DSRStats : public BRNElement
{

  public:
    class RouteInfo {
     public:
      Timestamp last_use;

      Vector<EtherAddress> route;
      Vector<uint32_t> link_metric;

      uint32_t metric;
      uint32_t id;

      uint32_t count_usage;

      RouteInfo(const click_dsr_hop *dsr_hops, int hop_count, uint16_t rid): last_use(Timestamp::now()), metric(0), id(rid), count_usage(1) {
        for ( int h = 0; h < hop_count; h++ ) {
          metric += ntohs(dsr_hops[h].metric);
          route.push_back(EtherAddress(dsr_hops[h].hw.data));
        }
      }

      ~RouteInfo() {
        route.clear();
        link_metric.clear();
      }

      void inc_usage() {
        count_usage++;
        last_use = Timestamp::now();
      }
    };

    typedef Vector<RouteInfo*> RoutInfoList;
    typedef RoutInfoList::const_iterator RoutInfoListIter;

    class RouteEntry {
     public:
      EtherAddress src;
      EtherAddress dst;

      RoutInfoList routes;

      RouteEntry(EtherAddress src_ea, EtherAddress dst_ea) : src(src_ea), dst(dst_ea) {
        routes.clear();
      }

      ~RouteEntry() {
        for ( int r = 0; r < routes.size(); r++ ) {
          RouteInfo *ri = routes[r];
          delete ri;
        }

        routes.clear();
      }

      void insert(const click_dsr_hop *dsr_hops, int hop_count, uint16_t id) {
        routes.push_back(new RouteInfo(dsr_hops,hop_count,id));
      }
    };

    typedef Pair<EtherAddress,EtherAddress> EtherAddressPair;
    typedef HashMap<EtherAddressPair, RouteEntry*> RouteMap;
    typedef RouteMap::const_iterator RouteMapIter;

  public:
    DSRStats();
    ~DSRStats();

/*ELEMENT*/
    const char *class_name() const  { return "DSRStats"; }
    const char *routing_name() const { return "DSRStats"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void add_handlers();

    void push( int port, Packet *packet );

    bool same_route(RouteInfo *ri, const click_dsr_hop *dsr_hops, int hop_count);

    String read_stats();
    void reset();

    RouteMap route_map;

};

CLICK_ENDDECLS
#endif
