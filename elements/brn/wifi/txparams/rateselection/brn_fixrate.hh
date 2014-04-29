#ifndef BRNFIXRATE_HH
#define BRNFIXRATE_HH
#include <click/timer.hh>

#include "rateselection.hh"
#include "elements/brn/wifi/brnwifi.hh"

#include "elements/brn/routing/broadcast/flooding/flooding.hh"
#include "elements/brn/routing/broadcast/flooding/flooding_helper.hh"

CLICK_DECLS

class BrnFixRate : public RateSelection
{
  public:
    BrnFixRate();
    ~BrnFixRate();

/*ELEMENT*/
    const char *class_name() const  { return "BrnFixRate"; }
    const char *name() const { return "BrnFixRate"; }
    void *cast(const char *name);

    const char *processing() const  { return AGNOSTIC; }

    const char *port_count() const  { return "0/0"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    void add_handlers();

    void assign_rate(struct rateselection_packet_info *rs_pkt_info, NeighbourRateInfo *);
    void process_feedback(struct rateselection_packet_info *, NeighbourRateInfo *) {};

    String print_neighbour_info(NeighbourRateInfo *nri, int tabs);

    int get_adjust_period() { return -1; }

    bool _set_rates;
    bool _set_power;

    uint8_t _rate0,_rate1,_rate2,_rate3;
    uint8_t _tries0,_tries1,_tries2,_tries3;
    bool _mcs0,_mcs1,_mcs2,_mcs3;
    int _bw0,_bw1,_bw2,_bw3;
    bool _sgi0,_sgi1,_sgi2,_sgi3;
    bool _gf0,_gf1,_gf2,_gf3;
    int _fec0,_fec1,_fec2,_fec3;
    bool _sp0,_sp1,_sp2,_sp3;
    bool _stbc0,_stbc1,_stbc2,_stbc3;

    uint32_t _wifi_extra_flags;
    struct brn_click_wifi_extra_extention _mcs_flags;

    uint16_t _power;

};

CLICK_ENDDECLS
#endif
