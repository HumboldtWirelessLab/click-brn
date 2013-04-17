#ifndef CLICK_ANALYZE_PHANTOM_HH
#define CLICK_ANALYZE_PHANTOM_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <click/packet.hh>
#include <clicknet/wifi.h>

#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brnelement.hh"

#define INIT      -1
#define QUEUE_LEN  2



CLICK_DECLS


struct pkt_q_entry {
  Packet *p;
  u_int32_t err_type;
  Timestamp ts;
};



class AnalyzePhantom : public BRNElement {

public:
  struct pkt_q_entry pkt_q[QUEUE_LEN];

  AnalyzePhantom();
  ~AnalyzePhantom();

  const char *class_name() const { return "AnalyzePhantom"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &conf, ErrorHandler* errh);
  Packet *simple_action(Packet *p);


private:
  void insert_pkt(Paket *p, u_int32_t err_type);
  void analyze();

};


CLICK_ENDDECLS

#endif /* CLICK_ANALYZE_PHANTOM_HH */
