/*
 *  
 */

#include <click/config.h>
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>

#include <clicknet/wifi.h>
#include <clicknet/llc.h>

#include <elements/wifi/wirelessinfo.hh>

#include "brn2_setrtscts.hh"



CLICK_DECLS


	Brn2_SetRTSCTS::Brn2_SetRTSCTS() : _winfo(0){
		_debug = 4;
		_rts = false;
		_mode = WIFI_FC1_DIR_NODS;
	}


	Brn2_SetRTSCTS::~Brn2_SetRTSCTS() {

	}


  
	int Brn2_SetRTSCTS::initialize(ErrorHandler *) {
		return 0;
	}

	int Brn2_SetRTSCTS::configure(Vector<String> &, ErrorHandler *) 
	{
		return 0;
	}


	/*
	int Brn2_SetRTS::configure(Vector<String> &conf, ErrorHandler *errh)
	{
    	return Args(conf, this, errh).read_mp("RTS", _rts).complete();
	}
	*/

	Packet *Brn2_SetRTSCTS::simple_action(Packet *p)
	{
	  if (p){
		dest_test(p);

		struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
		ceh->magic = WIFI_EXTRA_MAGIC;
		rts_cts_decision();
		if (rts_get()) {
			ceh->flags |= WIFI_EXTRA_DO_RTS_CTS;
		} else {
	        	ceh->flags &= ~WIFI_EXTRA_DO_RTS_CTS;
    		}
	 }
	 return p;
	}

	enum {brn2_H_RTSCTS};

	static String brn2_SetRTSCTS_read_param(Element *e, void *thunk)
	{
	 	Brn2_SetRTSCTS *td = (Brn2_SetRTSCTS *)e;
		switch ((uintptr_t) thunk) {
			case brn2_H_RTSCTS:
			    return String(td->rts_get()) + "\n";
  			default:
			    return String();
  		}

	}

	static int brn2_SetRTSCTS_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
	{
	  Brn2_SetRTSCTS *f = (Brn2_SetRTSCTS *)e;
	  String s = cp_uncomment(in_s);
	  switch((intptr_t)vparam) {
	  case brn2_H_RTSCTS: {
	    unsigned m;
	    if (!IntArg().parse(s, m))
	      return errh->error("stepup parameter must be unsigned");
	    f->rts_set(m);
	    break;
	  }
	  }
	    return 0;

	}
	void Brn2_SetRTSCTS::add_handlers()
	{
	  add_read_handler("rts", brn2_SetRTSCTS_read_param, brn2_H_RTSCTS);
	  add_write_handler("rts", brn2_SetRTSCTS_write_param, brn2_H_RTSCTS);
	}

	bool Brn2_SetRTSCTS::is_number_in_random_range(unsigned int value)
	{
		int random_number = click_random(0, 100); //generate random number in range [0,100]
		if (value <= random_number) return true;
		else {
			return false;
		}
	}
	/* true = on, else off*/
	void Brn2_SetRTSCTS::rts_cts_decision() 
	{
		int value = 5;//hier von hidden node abhängig
		if (is_number_in_random_range(value)) rts_set(true);
		else {
			rts_set(false);
		}	
	}

	bool Brn2_SetRTSCTS::rts_get()
	{
		return _rts;
	}
	void Brn2_SetRTSCTS::rts_set(bool value)
	{
		_rts = value;
	}

Packet * Brn2_SetRTSCTS::dest_test(Packet *p)
{

  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid = _winfo ? _winfo->_bssid : _bssid;

  uint16_t ethtype;
  WritablePacket *p_out = 0;

  if (p->length() < sizeof(struct click_ether)) {
    click_chatter("%{element}: packet too small: %d vs %d\n",
		  this,
		  p->length(),
		  sizeof(struct click_ether));

    p->kill();
    return 0;

  }

  click_ether *eh = (click_ether *) p->data();
  src = EtherAddress(eh->ether_shost);
  dst = EtherAddress(eh->ether_dhost);
// String s = "hallo:hallo";
//BRN_DEBUG("hallo:In Dest-Test: Destination: %s", s.c_str());


  memcpy(&ethtype, p->data() + 12, 2);

  p_out = p->uniqueify();
  if (!p_out) {
    return 0;
  }


  p_out->pull(sizeof(struct click_ether));
  p_out = p_out->push(sizeof(struct click_llc));

  if (!p_out) {
    return 0;
  }

  memcpy(p_out->data(), WIFI_LLC_HEADER, WIFI_LLC_HEADER_LEN);
  memcpy(p_out->data() + 6, &ethtype, 2);

  if (!(p_out = p_out->push(sizeof(struct click_wifi))))
      return 0;
  struct click_wifi *w = (struct click_wifi *) p_out->data();

  memset(p_out->data(), 0, sizeof(click_wifi));
  w->i_fc[0] = (uint8_t) (WIFI_FC0_VERSION_0 | WIFI_FC0_TYPE_DATA);
  w->i_fc[1] = 0;
  w->i_fc[1] |= (uint8_t) (WIFI_FC1_DIR_MASK & _mode);


 // BRN_DEBUG("In Dest-Test: Destination: %s; Source: %s", dst.unparse(), src.unparse());

  switch (_mode) {
  case WIFI_FC1_DIR_NODS:
	 BRN_DEBUG("Bin hier in WIFI_FC1_DIR_NODS");

    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
BRN_DEBUG("hallo:In Dest-Test: Destination: %s; Source: %s", dst.unparse().c_str(), src.unparse().c_str());

    break;
  case WIFI_FC1_DIR_TODS:
		 BRN_DEBUG("Bin hier in  WIFI_FC1_DIR_TODS");
    memcpy(w->i_addr1, bssid.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, dst.data(), 6);
    break;
  case WIFI_FC1_DIR_FROMDS:
		 BRN_DEBUG("Bin hier in  WIFI_FC1_DIR_FROMDS");
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, bssid.data(), 6);
    memcpy(w->i_addr3, src.data(), 6);
    break;
  case WIFI_FC1_DIR_DSTODS:
		 BRN_DEBUG("Bin hier in WIFI_FC1_DIR_DSTODS");
    /* XXX this is wrong */
    memcpy(w->i_addr1, dst.data(), 6);
    memcpy(w->i_addr2, src.data(), 6);
    memcpy(w->i_addr3, bssid.data(), 6);
    break;
  default:
 click_chatter("%{element}: invalid mode %d\n",
		  this,
		  _mode);
    p_out->kill();
    return 0;
  }
EtherAddress dst_1 = EtherAddress(w->i_addr1);
BRN_DEBUG("In Dest-Test: Destination: %s; ", dst.unparse().c_str());//, src.unparse().c_str());

//  BRN_DEBUG("In Dest-Test: Destination: %s; Source: %s", (w->i_addr1).unparse(),(w->i_addr2).unparse());
  return p_out;
}




CLICK_ENDDECLS
EXPORT_ELEMENT(Brn2_SetRTSCTS)
