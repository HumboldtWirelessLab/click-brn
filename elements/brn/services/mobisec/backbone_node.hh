/*
 * backbone_node.hh
 *
 *  Created on: 28.04.2012
 *      Author: aureliano
 */

#ifndef BackboneNode_HH_
#define BackboneNode_HH_


#include <string>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/routing/identity/brn2_nodeidentity.hh"
#include "elements/standard/suppressor.hh"
#include "elements/wifi/wepencap.hh"
#include "elements/wifi/wepdecap.hh"

#include "kdp.hh"
#include "keymanagement.hh"

CLICK_DECLS

class BackboneNode : public BRNElement {
public:
	BackboneNode();
	~BackboneNode();

	const char *class_name() const { return "BackboneNode"; }
	const char *port_count() const { return "1/1"; }
	const char *processing() const { return PUSH; }
	void push(int port, Packet *p);

	int configure(Vector<String> &conf, ErrorHandler *errh);
	bool can_live_reconfigure() const	{ return false; }
	int initialize(ErrorHandler *);

	void snd_kdp_req();

	void jmp_next_session();
	static void session_trigger(Timer *, void *element) { (reinterpret_cast<BackboneNode *>(element))->jmp_next_session(); }

	void jmp_next_epoch();
	static void epoch_trigger(Timer *, void *element) { (reinterpret_cast<BackboneNode *>(element))->jmp_next_epoch(); }

	static void kdp_trigger(Timer *, void *element) { (reinterpret_cast<BackboneNode *>(element))->snd_kdp_req(); }

	void reset_key_material();

	String stats();

private:
	BRN2NodeIdentity *_me;

	Element *_tls;
	EtherAddress _ks_addr;

	Element *_wifidev_client;
	Element *_ap_q;
	Element *_client_q;

	// Control of key usage in wep module
	Element *_wepencap;
	Element *_wepdecap;

	// Control of packet flow dependent of authentication status of the node
	Element *_dev_control_up;
	Element *_dev_control_down;
	Element *_dev_control_down2;

	int _start_time;

	enum dev_type {dev_ap, dev_client};

	// Parameters to control the refreshing of key material
	Timer session_timer; 	// Controls installation of keys
	Timer epoch_timer; 		// Controls the keylist refreshing
	Timer kdp_timer;		// Controls the kdp-req sending process
	int tolerance;			// Tolerated time for kdp-req in
							// order to receive the kdp-replies in time
	int backoff; 			// Time between resending a new kdp without wep encryption
	int retry_cnt_down;		// The retry countdown indicates the execution of a kdp retry

	int req_id;

	KeyManagement keyman;
	KeyManagement BUF_keyman;	// a change between two subsequent epochs need a buffering structure

	// Parameter to define the security level
	enum proto_type _protocol_type;
	int _key_timeout;

	// Variables to collect statistical information
	bool bb_status;					// true=backbone_node, false=not_in_backbone
	int bb_join_cnt;				// backbone join counter
	int kdp_retry_cnt;
	int key_inst_cnt;				// key installation counter
	int cnt_bb_entrypoints;
	int cnt_bb_exitpoints;

	void handle_kdp_reply(Packet *);
	void switch_wep(String);
	void switch_dev(enum dev_type type);

	void add_handlers();
};



CLICK_ENDDECLS
#endif /* BackboneNode_HH_ */
