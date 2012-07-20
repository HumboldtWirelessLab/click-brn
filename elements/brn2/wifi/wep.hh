/*
 * wep.hh
 *
 *  Created on: 22.06.2012
 *      Author: kuehne@informatik.hu-berlin.de
 */

#ifndef WEP_HH_
#define WEP_HH_

#include <click/packet_anno.hh>

CLICK_DECLS

/* With painting wep packets we gain packet oriented control over wep encryption.
 * This is useful, if we want wep-module to be active, but some packet we might
 * want to send unencrypted. */
enum color {WEP_GREEN, 		// color for WEP-packets using encryption
			WEP_YELLOW, 	// color for packets not using encryption
			WEP_RED			// color for failed WEP-packets (e.g. decryption failed)
			};

CLICK_ENDDECLS
#endif /* WEP_HH_ */
