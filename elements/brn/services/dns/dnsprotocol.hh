#ifndef CLICK_BRNDNSPROTOCOL_HH
#define CLICK_BRNDNSPROTOCOL_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>

CLICK_DECLS

#define DNS_STANDARD_QUERY 0x0100
#define DNS_STANDARD_REPLY (1 << 16)


struct dns_header {
  uint16_t id;     //OCTET 1,2   ID
  uint16_t flags;  //OCTET 3,4   QR(1 bit)+OPCODE(4 bit)+AA(1 bit)+TC(1 bit)+RD(1 bit)+RA(1 bit) + Z(3 bit) + RCODE(4 bit)
  uint16_t qdcount;//OCTET 5,6   QDCOUNT
  uint16_t ancount;//OCTET 7,8   ANCOUNT
  uint16_t nscount;//OCTET 9,10  NSCOUNT
  uint16_t arcount;//OCTET 11,12 ARCOUNT
} CLICK_SIZE_PACKED_ATTRIBUTE;

#define DNS_HEADER_SIZE sizeof(dns_header)

class DNSProtocol : public Element {
 public:

  DNSProtocol();
  ~DNSProtocol();

  const char *class_name() const	{ return "DNSProtocol"; }

  static int set_dns_header(Packet *p, uint16_t _id, uint16_t flags, uint16_t qdcount,
                                       uint16_t ancount, uint16_t nscount, uint16_t arcount);

  static WritablePacket *new_dns_question(String qname, uint16_t qtype, uint16_t qclass);
  static WritablePacket *new_dns_answer( String qname, uint16_t qtype, uint16_t qclass,
                                         uint32_t ttl, uint16_t rdlength, void *rdata);

  static WritablePacket *dns_question_to_answer(Packet *p, void *name, uint16_t name_len, uint16_t rtype, uint16_t rclass, uint32_t ttl, uint16_t rdlength, void *rdata);
  static char* get_name(Packet *p);
  static struct dns_header* get_dns_header(Packet *p);
  static bool isInDomain(String name, String domain );

  static const unsigned char *get_rddata(Packet *p, uint16_t *rdlength);


};

CLICK_ENDDECLS
#endif
