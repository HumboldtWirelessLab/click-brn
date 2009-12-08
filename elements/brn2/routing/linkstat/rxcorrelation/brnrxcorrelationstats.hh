#ifndef BRNRXCORRELATIONSTATS_HH
#define BRNRXCORRELATIONSTATS_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <clicknet/ether.h>
#include <click/bighashmap.hh>
#include <click/vector.hh>

#include "brnrxcorrelation.hh"

CLICK_DECLS

/*
 * =c
 * BrnRXCorrelationStats()
 * =s
 *
 * =d
 */

class BrnRXCorrelationStats : public Element {

 public:
  BrnRXCorrelationStats();
  ~BrnRXCorrelationStats();
  //
  //methods
  //

  const char *class_name() const  { return "BrnRXCorrelationStats"; }
  const char *processing() const  { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  String printSinglePerInfo();
  String printPairPerInfo();

  int _debug;

 private:

  BrnRXCorrelation *_rxcorr;

  int _note_lp;

  int calculatedPER(BrnRXCorrelation::NeighbourInfo *cand);
  int calculatedPERPair(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB);
  int calculatedPERIndepend(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB);
  int calculatedPERDependPos(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB);
  int calculatedPERDependNeg(BrnRXCorrelation::NeighbourInfo *candA, BrnRXCorrelation::NeighbourInfo *candB);
};

CLICK_ENDDECLS
#endif
