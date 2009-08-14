#include <click/config.h>

#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/etheraddress.hh>

#include "brncanselevaluation.hh"
CLICK_DECLS

/*
-Linkprobenummber
-Number of Neighbors (n)
-For each (n) neighbor
  >>Number of linkprobes (x)
  >>1.linkprobes
  >>(x-1) offsets
*/

#define MODESENDER 0
#define MODERECEIVER 1


/*
TODO
-remove linkprobeid-overflows ( "just" unsigned int ): add_recv_linkprobe,...
-send cand hints what you know (_he_knows_what_i_know)
-remove candidates which are not available anymore
*/

BRNCandidateSelectorEvaluation::BRNCandidateSelectorEvaluation()
{
}

BRNCandidateSelectorEvaluation::~BRNCandidateSelectorEvaluation()
{
}

int
BRNCandidateSelectorEvaluation::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int issender,isreceiver;

  issender = isreceiver = 0;

  if (cp_va_kparse(conf, this, errh,
      "ETHERADDRESS", cpkP+cpkM, cpEtherAddress, /*"etheraddress",*/ &_me,
      "SENDER", cpkP+cpkM, cpInteger, /*"is sender",*/ &issender,
      "RECEIVER", cpkP+cpkM, cpInteger, /*"is receiver",*/ &isreceiver,
      "DEBUG", cpkP+cpkM, cpInteger, /*"debug",*/ &_debug,
      cpEnd) < 0)
       return -1;

  if ( ( isreceiver == 0 ) || ( issender == 1 ) ) _mode = MODESENDER;
  if ( isreceiver == 1 ) _mode = MODERECEIVER;



  return 0;
}

int
BRNCandidateSelectorEvaluation::initialize(ErrorHandler *)
{
  if ( _mode == MODERECEIVER )
    click_chatter("Mode: Receiver");
  else
    click_chatter("Mode: Sender");

  _debug = 0;
  return 0;
}

void
BRNCandidateSelectorEvaluation::push( int /*port*/, Packet *packet )
{
  uint32_t packet_id;
  uint8_t ui8_cssize, ui8_per;

  uint8_t *new_packet_data = (uint8_t*)packet->data();
  uint16_t data_offset;
  uint8_t inc_cs;
  uint8_t per, ui8_val;
  int int_per;

  EtherAddress cand;

  StringAccum sa;

  data_offset = 0;
  inc_cs = 0;

  memcpy(&packet_id, &(new_packet_data[data_offset]), sizeof(packet_id));
  data_offset += sizeof(packet_id);

  memcpy(&ui8_cssize, &(new_packet_data[data_offset]), sizeof(ui8_cssize));
  data_offset += sizeof(ui8_cssize);

  for (int i = 0; i < ui8_cssize; i++)
  {
    cand = EtherAddress(&(new_packet_data[data_offset]));
    data_offset += 6;

    memcpy(&per, &(new_packet_data[data_offset]), sizeof(per));
    data_offset += sizeof(per);
    int_per = per;

    if ( cand == _me ) inc_cs = 1;
    sa << cand.unparse() << " (Per:" << int_per << ")";
    if ( (i + 1) != ui8_cssize ) sa << ",";
  }

  if ( ui8_cssize == 0 ) sa << " ";

  memcpy(&ui8_per, &(new_packet_data[data_offset]), sizeof(ui8_per) );
  data_offset += sizeof(ui8_per);

  sa << " Next: ";

  for (int i = 0; i < ui8_cssize; i++)
  {
    cand = EtherAddress(&(new_packet_data[data_offset]));
    data_offset += 6;

    memcpy(&ui8_val, &(new_packet_data[data_offset]), sizeof(ui8_val) );
    data_offset += sizeof(ui8_val);
    int_per = ui8_val;

    sa << cand.unparse() << " (Per:" << int_per << ")";
    if ( (i + 1) != ui8_cssize ) sa << ",";
  }

  click_chatter("BRNExORPacketReceiver: %s PacketID: %d CSsize: %d Inc_CS: %d Per: %d CS: %s",_me.unparse().c_str(),packet_id,ui8_cssize,inc_cs,ui8_per,sa.take_string().c_str());

  packet->kill();
 
}

static int 
write_handler_debug(const String &in_s, Element *e, void *,ErrorHandler *errh)
{
  BRNCandidateSelectorEvaluation *brncansec = (BRNCandidateSelectorEvaluation*)e;
  String s = cp_uncomment(in_s);

  int debug;

  if (!cp_integer(s, &debug))
    return errh->error("Debug is Integer");

  brncansec->_debug = debug;

  return 0;
}

/**
 * Read the debugvalue
 * @param e 
 * @param  
 * @return 
 */
static String
read_handler_debug(Element *e, void *)
{
  BRNCandidateSelectorEvaluation *brn2debug = (BRNCandidateSelectorEvaluation*)e;

  return String(brn2debug->_debug);
}

void
BRNCandidateSelectorEvaluation::add_handlers()
{
  add_read_handler("debug", read_handler_debug, 0);
  add_write_handler("debug", write_handler_debug, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRNCandidateSelectorEvaluation)
