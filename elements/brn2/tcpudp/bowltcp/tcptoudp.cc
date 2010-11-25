#include <click/config.h>
#include "tcptoudp.hh"
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>

CLICK_DECLS

Packet * 
TCPtoUDP::simple_action(Packet *p_in) { 
	
    	WritablePacket *p = p_in->uniqueify(); 
	click_ip *iph = p->ip_header(); 
	click_tcp *tcph = p->tcp_header(); 
	click_udp *udph;

	int hlen = 0;
	int dlen = 0;
	uint32_t ports; 
	memcpy(&ports, &tcph->th_sport, 4); 
	
	click_ip iptmp = *iph; 
	

	hlen -= p->network_header_length() 
	    + (tcph->th_off << 2); 
	dlen = p->network_length() + hlen;
	hlen += sizeof(click_ip) + sizeof(click_udp); 
	
	if ( hlen > 0 ) 
	    p = p->push(hlen);
	if (hlen < 0 ) 
	    p->pull(-hlen); 

	p->set_network_header(p->data(), sizeof(click_ip)); 

	iph = reinterpret_cast<click_ip *>(p->data()); 
	udph = reinterpret_cast<click_udp *>(iph + 1); 

	memcpy(iph, &iptmp, sizeof(click_ip)); 
	memcpy(&(udph->uh_sport), &ports, 4); 
	udph->uh_ulen = htons(dlen + sizeof(click_udp));  
	iph->ip_len = htons(p->length()); 
	iph->ip_p = IP_PROTO_UDP; 
	
	return p; 
}


CLICK_ENDDECLS
EXPORT_ELEMENT(TCPtoUDP)

