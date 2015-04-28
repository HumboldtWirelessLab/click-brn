#include <click/config.h>
#include "brn2_modelflow.hh"

#include <click/etheraddress.hh>
#include <clicknet/ether.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/timer.hh>
#include <click/packet.hh>

#include "elements/brn/brnprotocol/brnpacketanno.hh"
#include "elements/brn/brnprotocol/brnprotocol.hh"

CLICK_DECLS

BRN2ModelFlow::BRN2ModelFlow()
{
  BRNElement::init();
}

BRN2ModelFlow::~BRN2ModelFlow()
{
}

int BRN2ModelFlow::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String _size_distribution;
  String _time_distribution;

  if (cp_va_kparse(conf, this, errh,
      "SIZE", cpkP+cpkM, cpString, &_size_distribution,
      "TIME", cpkP+cpkM, cpString, &_time_distribution,
      cpEnd) < 0)
    return -1;

  Vector<String> _size_values;
  cp_spacevec(_size_distribution, _size_values);

  if ( _size_values.size() % 3 != 0 )
    click_chatter("Error in SizeVector");

  packet_size_vector_len = 0;
  int _packet_size;
  int _packet_size_range;
  int _packet_commonness;

  for ( int i = 0; i < _size_values.size(); i += 3) {
    cp_integer(_size_values[i], &_packet_size);
    cp_integer(_size_values[i+1], &_packet_size_range);
    cp_integer(_size_values[i+2], &_packet_commonness);
    packet_size_vector_len += _packet_commonness;
  }

  packet_sizes = new uint16_t[packet_size_vector_len];
  int _size_vector_index = 0;

  for ( int i = 0; i < _size_values.size(); i += 3) {
    cp_integer(_size_values[i], &_packet_size);
    cp_integer(_size_values[i+1], &_packet_size_range);
    cp_integer(_size_values[i+2], &_packet_commonness);

    for ( int j = 0; j < _packet_commonness; j++ ) {  //TODO: replace by memset
      packet_sizes[_size_vector_index] = _packet_size;
      _size_vector_index++;
    }
  }

  Vector<String> _time_values;
  cp_spacevec(_time_distribution, _time_values);

  if ( _time_values.size() % 2 != 0 )
    click_chatter("Error in TimeVector");

  packet_time_vector_len = 0;
  int _packet_time;
  int _packet_time_commonness;

  for ( int i = 0; i < _time_values.size(); i += 2) {
    cp_integer(_time_values[i], &_packet_time);
    cp_integer(_time_values[i+1], &_packet_time_commonness);
    packet_time_vector_len += _packet_time_commonness;
  }

  interpacket_time = new uint32_t[packet_time_vector_len];
  int _time_vector_index = 0;

  for ( int i = 0; i < _time_values.size(); i += 2) {
    cp_integer(_time_values[i], &_packet_time);
    cp_integer(_time_values[i+1], &_packet_time_commonness);

    for ( int j = 0; j < _packet_time_commonness; j++ ) {  //TODO: replace by memset
      interpacket_time[_time_vector_index] = _packet_time;
      _time_vector_index++;
    }
  }

  return 0;
}

int BRN2ModelFlow::initialize(ErrorHandler *)
{
  click_brn_srandom();

  return 0;
}

int
BRN2ModelFlow::getNextPacketSize() {
  return ( packet_sizes[ click_random() % packet_size_vector_len] );
}

int32_t
BRN2ModelFlow::getNextPacketTime() {
  return ( interpacket_time[ click_random() % packet_time_vector_len] );
}

enum {
  H_TXSTATS_SHOW,
};

static String
BRN2ModelFlow_read_param(Element */*e*/, void *thunk)
{
//  BRN2ModelFlow *sf = (BRN2ModelFlow *)e;

  switch ((uintptr_t) thunk) {
    case H_TXSTATS_SHOW: {
      StringAccum sa;
      sa << "Stats:";
      return sa.take_string();
    }
    default:
      return String();
  }
}

void BRN2ModelFlow::add_handlers()
{
  add_read_handler("txstats", BRN2ModelFlow_read_param, (void *)H_TXSTATS_SHOW);
}

EXPORT_ELEMENT(BRN2ModelFlow)
CLICK_ENDDECLS
