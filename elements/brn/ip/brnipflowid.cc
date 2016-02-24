#include <click/config.h>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <clicknet/tcp.h>
#include <click/packet.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include "brnipflowid.hh"

CLICK_DECLS

BRNIPFlowID::BRNIPFlowID(const Packet *p)
{
  const click_ip *iph = p->ip_header();
  const click_udp *udph = p->udp_header();
  assert(p->has_network_header() && p->has_transport_header() && IP_FIRSTFRAG(iph));

  _saddr = IPAddress(iph->ip_src.s_addr);
  _daddr = IPAddress(iph->ip_dst.s_addr);
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

BRNIPFlowID::BRNIPFlowID(const click_ip *iph)
{
  assert(iph && IP_FIRSTFRAG(iph));
  const click_udp *udph = reinterpret_cast<const click_udp *>(reinterpret_cast<const unsigned char *>(iph) + (iph->ip_hl << 2));

  _saddr = IPAddress(iph->ip_src.s_addr);
  _daddr = IPAddress(iph->ip_dst.s_addr);
  _sport = udph->uh_sport;	// network byte order
  _dport = udph->uh_dport;	// network byte order
}

String
BRNIPFlowID::unparse() const
{
  const unsigned char *p = (const unsigned char *)&_saddr;
  const unsigned char *q = (const unsigned char *)&_daddr;
  String s;
  char tmp[128];
  sprintf(tmp, "(%hhu.%hhu.%hhu.%hhu, %hu, %hhu.%hhu.%hhu.%hhu, %hu)",
	  p[0], p[1], p[2], p[3], ntohs(_sport),
	  q[0], q[1], q[2], q[3], ntohs(_dport));
  return String(tmp);
}

StringAccum &
operator<<(StringAccum &sa, const BRNIPFlowID &flow_id)
{
  return (sa << flow_id.unparse());
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(BRNIPFlowID)
