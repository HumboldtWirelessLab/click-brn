#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include "flowcontrolinfo.hh"

CLICK_DECLS

FlowControlInfo::FlowControlInfo():
  _window_start_index(0),
  _window_next_index(0),
  _min_seq(0),
  _next_seq(0),
  _rtt(0)
{
  _flowid = 0;
  _max_window_size = DEFAULT_MAX_WINDOW_SIZE;
  _cur_window_size = _max_window_size;
  _packet_window = new Packet*[DEFAULT_MAX_WINDOW_SIZE];
  _ack_window = new bool[DEFAULT_MAX_WINDOW_SIZE];
  _packet_times = new Timestamp[DEFAULT_MAX_WINDOW_SIZE];

  for(int i = 0; i < DEFAULT_MAX_WINDOW_SIZE; i++) {
    _packet_window[i] = NULL;
    _ack_window[i] = false;
    _packet_times[i] = Timestamp::now();
  }
}

FlowControlInfo::FlowControlInfo(uint16_t flowid):
  _window_start_index(0),
  _window_next_index(0),
  _min_seq(0),
  _next_seq(0),
  _rtt(0)
{
  _flowid = flowid;
  _max_window_size = DEFAULT_MAX_WINDOW_SIZE;
  _cur_window_size = _max_window_size;
  _packet_window = new Packet*[DEFAULT_MAX_WINDOW_SIZE];
  _ack_window = new bool[DEFAULT_MAX_WINDOW_SIZE];
  _packet_times = new Timestamp[DEFAULT_MAX_WINDOW_SIZE];

  for(int i = 0; i < DEFAULT_MAX_WINDOW_SIZE; i++) {
    _packet_window[i] = NULL;
    _ack_window[i] = false;
    _packet_times[i] = Timestamp::now();
  }
}

FlowControlInfo::FlowControlInfo(uint16_t flowid, uint16_t max_window_size):
  _window_start_index(0),
  _window_next_index(0),
  _min_seq(0),
  _next_seq(0),
  _rtt(0)
{
  _flowid = flowid;
  _max_window_size = max_window_size;
  _cur_window_size = _max_window_size;
  _packet_window = new Packet*[max_window_size];
  _ack_window = new bool[max_window_size];
  _packet_times = new Timestamp[max_window_size];

  for(int i = 0; i < max_window_size; i++) {
    _packet_window[i] = NULL;
    _ack_window[i] = false;
    _packet_times[i] = Timestamp::now();
  }
}

FlowControlInfo::~FlowControlInfo()
{
  clear();
  delete[] _packet_window;
  delete[] _ack_window;
  delete[] _packet_times;
}

int
FlowControlInfo::insert_packet(Packet *p)
{
  //click_chatter("Insert Packet: %p",p);
  
  if ( _packet_window[_window_next_index] != NULL ) return -1;
  if ( ((_window_next_index+1)%_cur_window_size) == _window_start_index ) return -1;

  _packet_window[_window_next_index] = p;
  _ack_window[_window_next_index] = false;
  _packet_times[_window_next_index] = Timestamp::now();

  _window_next_index = ((_window_next_index+1)%_cur_window_size);

  return _next_seq++;
}

bool
FlowControlInfo::has_packet(uint16_t seq)
{
  return ((_packet_window[(seq%_cur_window_size)] != NULL) && //no packet for seq
          ((_min_seq + _cur_window_size) > seq));             //is possible to have it
}

bool
FlowControlInfo::packet_fits_in_window(uint16_t seq)
{
  return !((seq < _min_seq) || ((_min_seq+_cur_window_size) <= seq));
}

int
FlowControlInfo::insert_packet(Packet *p, uint16_t seq)
{
  if ((seq < _min_seq) || ((_min_seq+_cur_window_size) <= seq)) return -1;
  if (_packet_window[(seq%_cur_window_size)] != NULL) return -2;

  if (_packet_window[(seq%_cur_window_size)] != NULL) {
    click_chatter("Receive dupl. of packet");
  }

  _packet_window[(seq%_cur_window_size)] = p;
  _ack_window[(seq%_cur_window_size)] = true;
  _packet_times[(seq%_cur_window_size)] = Timestamp::now();

  return 0;
}

/******************************/
/*           RX               */

int32_t
FlowControlInfo::max_not_acked_seq()
{
  int i;
  for ( i = 0; i < _cur_window_size; i++) {
    if ( ! _ack_window[(_window_start_index + i)%_cur_window_size]) break;
  }

  return _min_seq + i - 1;
}

Packet*
FlowControlInfo::get_and_clear_acked_seq()
{
  Packet *p = NULL;
  if ( _ack_window[_window_start_index] ) {
    _ack_window[_window_start_index] = false;
    p = _packet_window[_window_start_index];
    _packet_window[_window_start_index] = NULL;
    _window_start_index=(_window_start_index+1)%_cur_window_size;
    _min_seq++;
  }

  return p;
}

void
FlowControlInfo::clear_packets_up_to_seq(uint16_t seq)
{
  for( ; _min_seq <= seq; _min_seq++) {
    _ack_window[_window_start_index] = false;
    if ( _packet_window[_window_start_index] != NULL ) {
      _packet_window[_window_start_index]->kill();
      _packet_window[_window_start_index] = NULL;
    }
    _window_start_index=(_window_start_index+1)%_cur_window_size;
  }
}

int32_t
FlowControlInfo::min_age_not_acked()
{
  int32_t res = -1;
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _max_window_size; i++) {
    if (_packet_window[i] != NULL) {
      if ( ((now - _packet_times[i]).msecval() < res) || (res == -1) )
        res = (now - _packet_times[i]).msecval();
    }
  }

  return res;
}

Packet *
FlowControlInfo::get_packet_with_max_age(int32_t age)
{
  Timestamp now = Timestamp::now();

  for(int i = 0; i < _max_window_size; i++) {
    if (_packet_window[i] != NULL) {
      if ((now - _packet_times[i]).msecval() > age) {
        //click_chatter("Age: %d I: %d p: %p",(now - _packet_times[i]).msecval(),i,_packet_window[i]);
        _packet_times[i] = now;
        return _packet_window[i]->clone();
      }
    }
  }

  //click_chatter("Nuescht");

  return NULL;
}

void
FlowControlInfo::clear()
{
  for(int i = 0; i < _max_window_size; i++) {
    if (_packet_window[i] != NULL) {
      _packet_window[i]->kill();
      _packet_window[i] = NULL;
    }
  }
}

String
FlowControlInfo::print_info()
{
  StringAccum sa;

  sa << "Flow: ID: " << (uint32_t)_flowid ;

  sa << " mws: " << _max_window_size;
  sa << " cws: " << _cur_window_size;

  sa << " wsi: " << _window_start_index;
  sa << " wni: " << _window_next_index;

  sa << " min_seq: " << _min_seq;
  sa << " next_seq: " << _next_seq;

  sa << " rtt: " << _rtt;

  return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(FlowControlInfo)

