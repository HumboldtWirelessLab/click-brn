#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "flowcontrolinfo.hh"

CLICK_DECLS

FlowControlInfo::FlowControlInfo() :
  _max_window_size(DEFAULT_MAX_WINDOW_SIZE),
  _cur_window_size(0),
  _window_start_index(0),
  _window_next_index(0),
  _min_seq(0),
  _next_seq(0),
  _rtt(0)
{
  FlowControlInfo(DEFAULT_MAX_WINDOW_SIZE);
}

FlowControlInfo::FlowControlInfo(uint16_t max_window_size):
  _window_start_index(0),
  _window_next_index(0),
  _min_seq(0),
  _next_seq(0),
  _rtt(0)
{
  _max_window_size = max_window_size;
  _cur_window_size = 0;
  _packet_window = new Packet*[max_window_size];
  _ack_window = new bool[max_window_size];

  for(int i = 0; i < max_window_size; i++) {
    _packet_window[i] = NULL;
    _ack_window[i] = false;
  }
}

FlowControlInfo::~FlowControlInfo()
{
  clear();
  delete _packet_window;
}

int
FlowControlInfo::insert_packet(Packet *p, uint16_t seq)
{
  if ( _packet_window[_window_next_index] == NULL ) return -1;
  
  _packet_window[_window_next_index] = p;
  _ack_window[_window_next_index] = false;
    
  return seq;
}
    
uint32_t
FlowControlInfo::non_acked_packets()
{
  return 0;
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

CLICK_ENDDECLS
ELEMENT_PROVIDES(FlowControlInfo)

