#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>

#include "brn2_crcerrorrecovery.hh"

CLICK_DECLS

BRN2CRCErrorRecory::BRN2CRCErrorRecory()
{
}

BRN2CRCErrorRecory::~BRN2CRCErrorRecory()
{
}

int
BRN2CRCErrorRecory::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LABEL", cpkP+cpkM, cpString, /*"label",*/ &_label,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
BRN2CRCErrorRecory::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
BRN2CRCErrorRecory::simple_action(Packet *p_in)
{
  uint8_t *packet_data = (uint8_t*)p_in->data();
  uint32_t i;
  int crc_count;
  StringAccum sa;
  uint32_t seq_num;

  memcpy(&seq_num, &(packet_data[2]), sizeof(seq_num));

  seq_num=ntohl(seq_num);

  crc_count = 0;
  for ( i = 4; i < p_in->length(); i++)
  {
    uint8_t next_byte = packet_data[i];
    if ( next_byte != 0 ) crc_count++;

    for(uint8_t bit = 128; bit > 0; bit = bit < 1)
    {
      sa << seq_num;
      if ( ( next_byte & bit ) == 1 ) sa << " 1\n";
      else sa << " 0\n";
    }
  }
  click_chatter("%s\n", sa.c_str());
  return p_in;
}


static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
BRN2CRCErrorRecory::add_handlers()
{
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2CRCErrorRecory)
