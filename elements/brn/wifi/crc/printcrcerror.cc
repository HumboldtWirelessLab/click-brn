#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>

#include "printcrcerror.hh"
#include <elements/brn/wifi/brnwifi.hh>

CLICK_DECLS

PrintCRCError::PrintCRCError():
  _offset(0),
  _bits(1),
  _pad(0)
{
}

PrintCRCError::~PrintCRCError()
{
}

int
PrintCRCError::configure(Vector<String> &conf, ErrorHandler* errh)
{
  _label = "";

  if (cp_va_kparse(conf, this, errh,
      "LABEL", cpkP+cpkM, cpString, &_label,
      "OFFSET", cpkP, cpInteger, &_offset,
      "BITS", cpkP, cpInteger, &_bits,
      "PAD", cpkP, cpInteger, &_pad,
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
  StringAccum sa;

  if ( 1/*ceh->flags & WIFI_EXTRA_RX_CRC_ERR*/  ) {

    if ( _label != "" ) {
      uint32_t seq_num;
      struct click_wifi_extra *ceh = (struct click_wifi_extra *)WIFI_EXTRA_ANNO(p_in);
      memcpy(&seq_num, &(packet_data[2]), sizeof(seq_num));
      seq_num=ntohl(seq_num);

      sa << _label << ": " << (uint32_t)ceh->rate << " " << seq_num << " " << p_in->length() << " : ";
    }

    uint32_t crc_count = 0;
    uint32_t bit_pos = 0;

    uint32_t i;
    for ( i = _offset; i < p_in->length(); i++) {
      //OMG: This code is pretty ugly.
      uint8_t next_byte = packet_data[i];
      if ( next_byte != 0 ) {
        crc_count++;

        if ( _bits == 8 ) sa << " 1";

      } else if ( _bits == 8 ) sa << " 0";

      if ( _bits == 1 ) {
        for(uint8_t bit = 128; bit > 0; bit = bit >> 1)
        {
          if ( ( next_byte & bit ) == 1 ) sa << " 1";
          else sa << " 0";
          bit_pos++;
        }
      }

    }

    if ( _pad != 0 )
      for ( ; i < _pad; i++) sa << " 9";

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
