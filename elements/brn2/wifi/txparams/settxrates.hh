#ifndef CLICK_SETTXRATES_HH
#define CLICK_SETTXRATES_HH
#include <click/element.hh>
#include <clicknet/ether.h>
#include "elements/brn2/wifi/brnwifi.hh"

CLICK_DECLS

class SetTXRates : public Element { public:

  SetTXRates();
  ~SetTXRates();

  const char *class_name() const	{ return "SetTXRates"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const	{ return true; }

  Packet *simple_action(Packet *);


  void add_handlers();

  bool _debug;
 private:

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

};

CLICK_ENDDECLS
#endif
