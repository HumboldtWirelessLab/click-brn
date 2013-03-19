#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "dnsprotocol.hh"

CLICK_DECLS

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

  WritablePacket *p = Packet::make(128, NULL, DNS_HEADER_SIZE + qname.length() + 1 + missing_dot_ext + 2 * sizeof(uint16_t), 32);    //new packets

  uint8_t *data = &(((uint8_t*)(p->data()))[DNS_HEADER_SIZE]);
  uint16_t *fields;
  char *len_field = NULL;

  if ( missing_dot_ext == 1 ) data[0] = '.';
  memcpy(&(data[missing_dot_ext]), qname.data(), qname.length());
  data[qname.length() + missing_dot_ext] = 0;

  int len = 0;
  for ( int i = 0; i < (qname.length() + missing_dot_ext); i++, len++ ) {
    if ( data[i] == '.' ) {
      if ( len_field != NULL ) 
        len_field[0] = (char)(len - 1);

      len_field = (char*)&(data[i]);
      len = 0;
    }
  }
  len_field[0] = ( len - 1);

  fields = (uint16_t*)&(data[qname.length() + 1 + missing_dot_ext]);
  fields[0] = htons(qtype);
  fields[1] = htons(qclass);

  return p;
}

WritablePacket *
DNSProtocol::new_dns_answer( String /*qname*/, uint16_t /*qtype*/, uint16_t /*qclass*/,
                                         uint32_t /*ttl*/, uint16_t /*rdlength*/, void */*rdata*/)
{
  return NULL;
}

WritablePacket *
DNSProtocol::dns_question_to_answer(Packet *p, void *name, uint16_t name_len, uint16_t rtype, uint16_t rclass, uint32_t ttl, uint16_t rdlength, void *rdata)
{
  int pointer = p->length();
  WritablePacket *q = p->put(name_len + sizeof(rclass) + sizeof(rtype) + sizeof(ttl) + sizeof(rdlength) + rdlength);

  if ( q != NULL ) {

    uint8_t *data = &((q->data())[pointer]);
    memcpy(data,name,name_len);

    uint16_t *data_16t = (uint16_t*)&(data[name_len]);
    data_16t[0] = htons(rtype);
    data_16t[1] = htons(rclass);

    uint32_t *data_32t = (uint32_t*)&(data_16t[2]);
    data_32t[0] = htonl(ttl);

    data_16t[4] = htons(rdlength);
    memcpy(&(data_16t[5]),rdata,rdlength);

    data_16t = (uint16_t*)q->data();
    data_16t[1] = data_16t[1] | htons(0x8080);
    data_16t[3] = htons(1);

  }

  return q;
}

char *
DNSProtocol::get_name(Packet *p)
{
  char *name = new char[p->length() - DNS_HEADER_SIZE - 3];

  if ( p->length() > DNS_HEADER_SIZE ) {

    uint8_t *data = (uint8_t*)p->data();
    memcpy(name,(char*)&(data[DNS_HEADER_SIZE]),p->length() - ( DNS_HEADER_SIZE + 4));
    name[p->length() - (DNS_HEADER_SIZE + 4)] = '\0';

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

const unsigned char *
DNSProtocol::get_rddata(Packet *p, uint16_t *rdlength)
{
  char *name = get_name(p);

  uint8_t *data = (uint8_t*)&(p->data()[DNS_HEADER_SIZE + strlen(name) + 1 + 2 * sizeof(uint16_t)]);
  uint16_t *data_16t = (uint16_t*)&(data[sizeof(uint16_t)]);

  *rdlength = ntohs(data_16t[4]);

  delete[] name;
  return (unsigned char*)&(data_16t[5]);
}

struct dns_header *
DNSProtocol::get_dns_header(Packet */*p*/)
{
  return NULL;
}

bool
DNSProtocol::isInDomain(String name, String domain )
{
  if ( name.length() < domain.length() ) return false;
  return ( memcmp( &(name.data()[name.length() - domain.length()]), domain.data(), domain.length()) == 0 );
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(DNSProtocol)

