/*
 * File:   nodechannelstats.hh
 * Author: kuehn@informatik.hu-berlin.de
 *
 * Created on 24. Juli 2012, 15:58
 */

#ifndef NODECHANNELSTATS_HH
#define	NODECHANNELSTATS_HH

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/args.hh>
#include <click/straccum.hh>
#if CLICK_NS
#include <click/router.hh>
#endif
#include "elements/brn/brnelement.hh"
#include "elements/brn/wifi/rxinfo/channelstats.hh"

#define ENDIANESS_TEST 0x1234;

CLICK_DECLS

class NodeChannelStats
{
  public:
    NodeChannelStats();
    NodeChannelStats(EtherAddress &ea);
    virtual ~NodeChannelStats();

    typedef HashMap<EtherAddress, struct neighbour_airtime_stats*> NeighbourStatsTable;
    typedef NeighbourStatsTable::const_iterator NeighbourStatsTableIter;

    void set_stats(struct airtime_stats &new_stats, uint16_t endianess);
    struct airtime_stats *get_airtime_stats();

    void add_neighbour_stats(EtherAddress &ea, struct neighbour_airtime_stats &stats);
    /// get Address of the neighbour who sent the neighbour_stats_table
    EtherAddress *get_address();
    NeighbourStatsTable get_neighbour_stats_table();

  private:
    uint16_t _endianess;
    bool _is_fix_endianess;
    struct airtime_stats stats;

    //Neighbour Airtime Stats of all neighbour's neighbours (one neighbour has many neighbours which stats it has)
    NeighbourStatsTable _n_stats;
    EtherAddress node;
};

CLICK_ENDDECLS
#endif	/* NODECHANNELSTATS_HH */

