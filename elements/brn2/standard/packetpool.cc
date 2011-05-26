#include <click/config.h>
#include <click/straccum.hh>

#include "packetpool.hh"

CLICK_DECLS

PacketPool::PacketPool(uint32_t capacity, uint32_t size_steps, uint32_t min_size, uint32_t max_size, uint32_t default_headroom, uint32_t default_tailroom ):
  _default_headroom(64),
  _default_tailroom(32),
  _inserts(0),
  _kills(0),
  _hits(0),
  _makes(0)
{
  _capacity = capacity;
  _size_steps = size_steps;
  _min_size = min_size;
  _max_size = max_size;
  _default_headroom = default_headroom;
  _default_tailroom = default_tailroom;


  _no_sizes = max_size / _size_steps;
  if ( (max_size % _size_steps) != 0 ) _no_sizes++;

  _packet_per_size = new uint32_t[_no_sizes];
  memset(_packet_per_size, 0, _no_sizes * sizeof(uint32_t));

  _packet_pool = new Packet*[_no_sizes * capacity];
  memset(_packet_pool, 0, _no_sizes * capacity * sizeof(Packet*));

  _max_size_index = _no_sizes - 1;
}

PacketPool::~PacketPool()
{
  for ( uint32_t i = 0; i < _max_size_index; i++ ) {
    if ( _packet_per_size[i] != 0 ) {
      for ( uint32_t j = 0; j < _packet_per_size[i]; j++ ) {
        _packet_pool[i * _capacity + j]->kill();
      }
    }
  }

  delete[] _packet_per_size;
  delete[] _packet_pool;
}

void
PacketPool::insert(Packet *p)
{
  //click_chatter("New packet: %d",p->buffer_length());
  if ( p->buffer_length() >= _min_size ) {
    uint32_t size_slot = (p->buffer_length() / _size_steps);

    if ( size_slot > _max_size_index ) size_slot  = _max_size_index;

    if ( _packet_per_size[size_slot] < _capacity ) {
      _packet_pool[size_slot * _capacity + _packet_per_size[size_slot]] = p;
      _packet_per_size[size_slot]++;
      _inserts++;
      return;
    }
  }

  _kills++;
  p->kill();

}

WritablePacket*
PacketPool::get(uint32_t size, uint32_t headroom, uint32_t tailroom)
{
  uint32_t total_size = size + headroom + tailroom;

  //click_chatter("Get_packet: %d", total_size);

  if ( total_size <= _max_size ) {
    uint32_t size_slot;

    if ( total_size < _min_size ) {
      size_slot = 0;
    } else {
      size_slot = (total_size/_size_steps) + 1;
    }

    //click_chatter("Slot: %d", size_slot);

    for ( uint32_t i = size_slot; i < _max_size_index; i++ ) {
      //click_chatter("Check slot %d: %d",i,_packet_per_size[i]);
      if ( _packet_per_size[i] != 0 ) {
        _packet_per_size[i]--;
        WritablePacket *p = _packet_pool[i * _capacity + _packet_per_size[i]]->uniqueify();
        _packet_pool[i * _capacity + _packet_per_size[i]] = NULL;

        /*adjust head- and tailroom */
        if ( p->headroom() > headroom ) {
          p->pull(p->headroom() - headroom);
        } else if ( p->headroom() < headroom ) {
          p = p->push(headroom - p->headroom());
        }

        if ( p->length() > size ) {
          p->take(p->length() - size);
        } else if ( p->length() < size ) {
          p = p->put(size - p->length());
        }

        _hits++;
        //click_chatter("Success");
        return p;
      }
    }
  }
  _makes++;

  return Packet::make(headroom, NULL, size, tailroom);
}

String
PacketPool::stats()
{
  StringAccum sa;

  sa << "<packetpool capacity=\"" << _capacity << "\" min_size=\"" << _min_size << "\" max_size=\"";
  sa << _max_size << "\" size_steps=\"" << _size_steps << "\" >\n\t<slots count=\"" << _no_sizes << "\" >\n";
  for ( uint32_t i = 0; i < _no_sizes; i++ ) {
    sa << "\t\t<slot min_size=\"" << i*_size_steps << "\" max_size=\"" << ((i+1)*_size_steps)-1;
    sa << "\" no_packets=\"" << _packet_per_size[i] << "\" />\n";
  }
  sa << "\t</slots>\n\t<stats inserts=\"" << _inserts << "\" kills=\"" << _kills;
  sa << "\" hits=\"" << _hits << "\" makes=\"" << _makes << "\" />\n</packetpool>\n";

  return sa.take_string();
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PacketPool)
