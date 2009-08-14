#ifndef BRNCANDIDATESELECTOREVALUATION_HH
#define BRNCANDIDATESELECTOREVALUATION_HH

#include <click/element.hh>
#include <click/timer.hh>
#include <click/timestamp.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/vector.hh>

CLICK_DECLS

/*
 * =c
 * BRNCandidateSelectorEvaluation()
 * =s
 * 
 * =d
 */

class BRNCandidateSelectorEvaluation : public Element {

 public:
  BRNCandidateSelectorEvaluation();
  ~BRNCandidateSelectorEvaluation();
  //
  //methods
  //

  const char *class_name() const  { return "BRNCandidateSelectorEvaluation"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/0"; }


  void push( int port, Packet *packet );

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  int initialize(ErrorHandler *);
  void add_handlers();

  String printCandidateInfo();

  int _debug;

 private:

  EtherAddress _me;
  int _mode;

};

CLICK_ENDDECLS
#endif
