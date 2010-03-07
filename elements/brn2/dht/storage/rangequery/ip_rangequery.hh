#ifndef DHT_IP_RANGE_QUERY_HH
#define DHT_IP_RANGE_QUERY_HH
#include <click/ipaddress.hh>

CLICK_DECLS

#define DEFAULT_LOCKTIME 3600

class IPRangeQuery
{
  public:

    IPAddress _net;
    IPAddress _mask;

    IPRangeQuery(IPAddress net, IPAddress mask);
    ~IPRangeQuery(){};

    IPAddress get_value_for_id(uint8_t *val);
    bool get_id_for_value(IPAddress ip, uint8_t *val);
};

CLICK_ENDDECLS
#endif
