/*
 * kdp.hh -- Key Distribution Protocol
 *
 *  Created on: 27.04.2012
 *      Author: aureliano
 *
 * This module only delivers the protocol messages.
 */

#ifndef KDP_HH_
#define KDP_HH_


#include <string>
#include <time.h>

#include <click/element.hh>
#include <click/confparse.hh>
#include <click/hashmap.hh>
#include "elements/brn2/brnelement.hh"

#include "keymanagement.hh"

CLICK_DECLS


enum proto_type {SERVER_DRIVEN, CLIENT_DRIVEN};

struct kdp_req {
	int req_id;
	EtherAddress node_id;
};

struct kdp_reply_header {
	time_t timestamp;
	int cardinality; // also interpretable as rounds
	int key_len;
	int seed_len;
};

class kdp : public BRNElement {
public:
	kdp();
	~kdp();

	const char *class_name() const { return "kdp"; }

	static WritablePacket *kdp_request_msg();

	/*
	 * Problem: Parametrisierung des Schlüsselmaterials.
	 * 1. Schwierigkeit: Informationen für die Abwehr von Replay-Angriffen auch Teil
	 * des Organisationsproblems.
	 * 2. Schwierigkeit: Es muss festgelegt werden, ob
	 * die Anzahl der Schlüssel dynamisch oder fest sein soll.  Dies wäre ein weiterer
	 * Parameter zur Erhöhung der Sicherheit oder aber der Effizienz.
	 *
	 * Eingabe-Parameter: Protokoll-Typ
	 * Ausgabe-Parameter: Paket mit Key-List-Kardinalität, Timestamp, key-timeout, Keys/Seed
	 *
	 * Lösung 1:
	 * 		kdp_reply(enum proto_type, ...)
	 *
	 * Lösung 2:
	 * 		kdp_reply_server_driven(int key_list_cardinality)
	 * 		kdp_reply_client_driven() // in dem pkt steht auch, wieviele Schlüssel aus dem seed abzuleiten sind
	 *
	 * Lösung 3:
	 * 		kdp_reply(config)
	 */

	/*
	 * Diese Funktion arbeitet transparent in dem Sinne, da sie bei einem bestimmten
	 * Protokoll-Typ ein entsprechendes Reply-Paket zurückgibt.
	 *
	 * Eingabe-Parameter: Protokoll-Typ
	 * Ausgabe-Parameter: Paket mit Key-List-Kardinalität, Timestamp, key-timeout, Keys/Seed
	 *
	 * Die Ausgabe-Parameter werden aus den im Speicher befindlichen Krypto-Daten
	 * ausgewählt.
	 * (crypo_generator() sorgt dafür, dass im Speicher die richtigen Krypto-Daten liegen)
	 *
	 * Wie sieht dieser speicher aus und in welchem Modul befindet er sich?
	 */
	static WritablePacket *kdp_reply_msg_cli_driv(crypto_info *);

	static WritablePacket *kdp_reply_msg_srv_driv(crypto_info *);
private:
};

CLICK_ENDDECLS
#endif /* KDP_HH_ */
