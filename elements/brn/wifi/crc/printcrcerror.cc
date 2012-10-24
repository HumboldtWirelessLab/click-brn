#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include "printcrcerror.hh"

CLICK_DECLS

PrintCRCError::PrintCRCError():
  _rate(0)
{
}

PrintCRCError::~PrintCRCError()
{
}

int
PrintCRCError::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "LABEL", cpkP+cpkM, cpString, /*"label",*/ &_label,
      "RATE", cpkP+cpkM, cpInteger, /*"rate",*/ &_rate,
      cpEnd) < 0)
       return -1;
  return 0;
}

int
PrintCRCError::initialize(ErrorHandler *)
{
  return 0;
}

Packet *
PrintCRCError::simple_action(Packet *p_in)
{
  uint8_t *packet_data = (uint8_t*)p_in->data();
  uint32_t i;
  int crc_count;
  uint8_t next_byte;
  StringAccum sa;
  uint32_t seq_num;
  uint32_t bit_pos;
  struct click_wifi_extra *ceh = (struct click_wifi_extra *) p_in->anno();

  if ( ( _rate == 0 ) || ( ( _rate != 0 ) && ( _rate == ceh->rate ) ) )
  {
    memcpy(&seq_num, &(packet_data[2]), sizeof(seq_num));

    seq_num=ntohl(seq_num);

    crc_count = 0;
    bit_pos = 0;

    for ( i = 4; i < p_in->length(); i++)
    {
      next_byte = packet_data[i];
      if ( next_byte != 0 ) crc_count++;

      for(uint8_t bit = 128; bit > 0; bit = bit >> 1)
      {
        sa << ceh->rate;
        sa << " ";

        sa << seq_num << " " << bit_pos;
        if ( ( next_byte & bit ) == 1 ) sa << " 1\n";
        else sa << " 0\n";
        bit_pos++;
      }
    }
    click_chatter("%s", sa.c_str());
  }

  return p_in;
}


static String
read_handler(Element *, void *)
{
  return "false\n";
}

void
PrintCRCError::add_handlers()
{
  add_read_handler("scheduled", read_handler, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintCRCError)
