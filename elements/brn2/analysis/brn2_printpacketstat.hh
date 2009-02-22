#ifndef CLICK_BRN2PRINTPACKETSTAT_HH
#define CLICK_BRN2PRINTPACKETSTAT_HH
#include <click/element.hh>
#include <click/string.hh>
CLICK_DECLS

/*
 * =c
 * BRN2PrintPacketStat([TAG] [, KEYWORDS])
 * =s debugging
 * =d
 * Assumes input packets are Wifi packets (ie a wifi_pkt struct from 
 * wifi.hh). Prints out a description of those packets.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =a
 * Print, Wifi
 */

class BRN2PrintPacketStat : public Element {

  String _label;

 public:

  BRN2PrintPacketStat();
  ~BRN2PrintPacketStat();

  const char *class_name() const		{ return "BRN2PrintPacketStat"; }
  const char *port_count() const		{ return "1/1"; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);

  Packet *simple_action(Packet *);
  String unparse_beacon(Packet *p);
  String reason_string(int reason);
  String status_string(int status);
  String get_ssid(u_int8_t *ptr);
  Vector<int> get_rates(u_int8_t *ptr);
  String rates_string(Vector<int> rates);
  String capability_string(int capability);
  bool _print_anno;
  bool _print_checksum;
  bool _timestamp;
};

CLICK_ENDDECLS
#endif
