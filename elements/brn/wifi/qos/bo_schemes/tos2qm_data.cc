#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include <clicknet/ether.h>

#include "tos2qm_data.hh"

CLICK_DECLS

Tos2QmData::Tos2QmData()
{
}

Tos2QmData::~Tos2QmData()
{
}

int
Tos2QmData::find_closest_size_index(int size)
{
  for ( int i = 0; i < T2QM_MAX_INDEX_MSDU_SIZE; i++ ) {
    if ( vector_msdu_sizes[i] >= size )
      return i;
  }

  return T2QM_MAX_INDEX_MSDU_SIZE - 1;
}

int
Tos2QmData::find_closest_rate_index(int rate)
{
  for ( int i = 0; i < T2QM_MAX_INDEX_RATES; i++ ) {
    if ( vector_rates_data[i] >= rate )
      return i;
  }

  return T2QM_MAX_INDEX_RATES - 1;
}

int
Tos2QmData::find_closest_no_neighbour_index(int no_neighbours) {
  if ( no_neighbours > T2QM_MAX_INDEX_NO_NEIGHBORS )
    return T2QM_MAX_INDEX_NO_NEIGHBORS-1;

  return no_neighbours - 1;
}

int
Tos2QmData::find_closest_per_index(int per) {
  for ( int i = 0; i < T2QM_MAX_INDEX_PACKET_LOSS; i++ ) {
    if ( _backoff_packet_loss[i] >= per )
      return i;
  }

  return T2QM_MAX_INDEX_PACKET_LOSS - 1;
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(Tos2QmData)
