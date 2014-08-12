#ifndef NEIGHBOURRATEINFO_HH
#define NEIGHBOURRATEINFO_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <clicknet/ether.h>

#include <click/timestamp.hh>

#include "elements/brn/wifi/brnavailablerates.hh"
#include "elements/brn/wifi/txparams/neighbourratestats.hh"

CLICK_DECLS

class NeighbourRateInfo {
  public:

    EtherAddress _eth;

    Vector<MCS> _rates;     // contains rates already devided by 10

    uint8_t _max_power;
    uint8_t _power;

    void *_rs_data;         // pointer which can be used for the rateselection

    static int _debug;

    Timestamp init_stamp;    // initial time stamp at the time of paket creation
    int stats_duration;

    NeighbourRateStats stats;
    Timestamp last_stats;

    NeighbourRateInfo();
    ~NeighbourRateInfo();

    NeighbourRateInfo(EtherAddress eth, Vector<MCS> rates, uint8_t max_power);

    /* H E L P E R */
    int rate_index(MCS rate);
    int rate_index(uint32_t rate);

    MCS pick_rate(uint32_t index);

    bool is_ht(uint32_t rate);

    /* helper + debug */
    int get_rate_index(uint32_t rate);

    void print_rates_tries(click_wifi_extra *ceh);

    void print_mcs_vector();

    void test_print();

};

typedef HashMap<EtherAddress, NeighbourRateInfo*> NeighborTable;
typedef NeighborTable::const_iterator NIter;


CLICK_ENDDECLS
#endif
