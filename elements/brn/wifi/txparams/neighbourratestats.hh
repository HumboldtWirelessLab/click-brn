#ifndef RATESSTATS_HH
#define RATESSTATS_HH

CLICK_DECLS

class NeighbourRateStats {

  public:

    int per;
    int throuput;
    int tries;

    NeighbourRateStats();
    NeighbourRateStats(int per, int tp, int tries);

    ~NeighbourRateStats();


};


CLICK_ENDDECLS
#endif
