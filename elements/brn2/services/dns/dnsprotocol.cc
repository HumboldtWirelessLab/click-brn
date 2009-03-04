#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "dnsprotocol.hh"
#include "elements/brn/brn.h"

CLICK_DECLS

DNSProtocol::DNSProtocol()
{
}

DNSProtocol::~DNSProtocol()
{
}

int
DNSProtocol::set_dns_header(Packet *p, uint16_t _id, uint16_t flags, uint16_t qdcount,
                            uint16_t ancount, uint16_t nscount, uint16_t arcount)
{
  uint16_t *data = (uint16_t*)p->data();

  data[0] = htons(_id);
  data[1] = htons(flags);
  data[2] = htons(qdcount);
  data[3] = htons(ancount);
  data[4] = htons(nscount);
  data[5] = htons(arcount);

  return 0;
}

WritablePacket *
DNSProtocol::new_dns_question(String qname, uint16_t qtype, uint16_t qclass)
{
  uint8_t missing_dot_ext = 0;
  if ( '.' != (char)(qname.data()[0])) missing_dot_ext = 1;
  WritablePacket *p = Packet::make(DNS_HEADER_SIZE + qname.length() + 1 + missing_dot_ext + 2 * sizeof(uint16_t));      //new packets
  uint8_t *data = &(((uint8_t*)(p->data()))[DNS_HEADER_SIZE]);
  uint16_t *fields;
  char *len_field = NULL;

  if ( missing_dot_ext == 1 ) data[0] = 3;
  memcpy(&(data[missing_dot_ext]), qname.data(), qname.length());
  data[qname.length() + missing_dot_ext] = 0;

  int len = 0;
  for ( int i = 0; i < (qname.length() + missing_dot_ext); i++, len++ )
    if ( data[i] == '.' ) {
      if ( len_field != NULL )
        len_field[0] = ( len - 1);

      len_field = (char*)&(data[i]);
      len = 0;
    }

  len_field[0] = ( len - 1);

  fields = (uint16_t*)&(data[qname.length() + 1 + missing_dot_ext]);
  fields[0] = htons(qtype);
  fields[1] = htons(qclass);

  return p;
}

WritablePacket *
DNSProtocol::new_dns_answer( String qname, uint16_t qtype, uint16_t qclass,
                                         uint32_t ttl, uint16_t rdlength, void *rdata)
{
}

WritablePacket *
DNSProtocol::dns_question_to_answer(uint32_t ttl, uint16_t rdlength, void *rdata)
{
}

char *
DNSProtocol::get_name(Packet *p)
{
  char *name = NULL;
  if ( p->length() > DNS_HEADER_SIZE ) {
    uint8_t *data = (uint8_t*)p->data();
    name = (char*)&(data[DNS_HEADER_SIZE]);

    int pointer = 0;
    int len = name[pointer];
    while ( len != 0 ) {
      name[pointer] = '.';
      pointer += len + 1;
      len = name[pointer];
    }
  }

  return name;
}

struct dns_header *
DNSProtocol::get_dns_header(Packet *p)
{
}

bool
DNSProtocol::isInDomain(String name, String domain )
{
  if ( name.length() < domain.length() ) return false;
  return ( memcmp( &(name.data()[name.length() - domain.length()]), domain.data(), domain.length()) == 0 );
}

CLICK_ENDDECLS
EXPORT_ELEMENT(DNSProtocol)

