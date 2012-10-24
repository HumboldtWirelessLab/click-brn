#include <click/config.h>
#include <clicknet/wifi.h>
#include "packetaggregation.hh"
#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/wifi/brnwifi.hh"
CLICK_DECLS

PacketAggregation::PacketAggregation()
{
}

PacketAggregation::~PacketAggregation()
{
}

Packet *
PacketAggregation::amsdu_aggregation(Vector<Packet *> *packets, uint32_t max_size, Vector<Packet *> *used_packets)
{
  uint32_t estimated_size = 32;
  int32_t used_packets_id[packets->size()];
  int32_t count_used_packets = 0;
  uint32_t packet_pos = 0;

  for ( int i = 0; i < packets->size(); i++ ) {
    // remove WifiHeader (-32) ; add SA,DA,Len,Type (LLC) (+20)
    if ( (estimated_size + ((*packets)[i]->length()) - 32 + 20) <= max_size ) {
      estimated_size += ((*packets)[i]->length()) - 32 + 20;
      if ( used_packets != NULL ) used_packets->push_back((*packets)[i]);
      count_used_packets++;
    }
  }

  estimated_size += count_used_packets*3; //worst padding

  if ( count_used_packets == 0 ) return NULL;

  // create new packet
  WritablePacket *p = WritablePacket::make(128,NULL, estimated_size, 32);

  // build wifiheader /take from the first used packet)
  struct click_wifi *click_wifi_h = (struct click_wifi *)((*used_packets)[0])->data();
  struct click_wifi *amsdu_wh = (struct click_wifi *)p->data();

  //TODO: check whether packets has not the "click standard" header
  memcpy((uint8_t*)amsdu_wh,(uint8_t*)click_wifi_h,sizeof(struct click_wifi));
  packet_pos += sizeof(struct click_wifi);

  struct wifi_qos_field *qos_field = (struct wifi_qos_field *)&(amsdu_wh[1]);
  struct wifi_n_ht_control *ht_ctrl = (struct wifi_n_ht_control *)&(qos_field[1]);
  packet_pos += sizeof(struct wifi_qos_field) + sizeof(struct wifi_n_ht_control);

  uint8_t *data = (uint8_t*)&(ht_ctrl[1]);

  for ( int i = 0; i < count_used_packets; i++) {
    struct wifi_n_msdu_header *msdu_h = (struct wifi_n_msdu_header *)data;
    click_wifi_h = (struct click_wifi *)((*packets)[used_packets_id[i]])->data();
    memcpy((uint8_t *)msdu_h->da, (uint8_t *)click_wifi_h->i_addr1, 6);
    memcpy((uint8_t *)msdu_h->sa, (uint8_t *)click_wifi_h->i_addr2, 6);
    msdu_h->len = htons(((*packets)[used_packets_id[i]])->length() - sizeof(struct click_wifi));

    memcpy((uint8_t*)&(msdu_h[1]), (uint8_t*)&(click_wifi_h[1]), ((*packets)[used_packets_id[i]])->length() - sizeof(struct click_wifi));
    packet_pos += sizeof(struct wifi_n_msdu_header) + ((*packets)[used_packets_id[i]])->length() - sizeof(struct click_wifi);

    if ( (packet_pos % 4) != 0 ) {
      memset(&(p->data()[packet_pos]),0,(packet_pos % 4));
      packet_pos += (packet_pos % 4);
    }
    data = &(p->data()[packet_pos]);
  }

  for ( int i = count_used_packets; i >= 0; i++ ) {
    if ( used_packets == NULL ) ((*packets)[used_packets_id[i]])->kill();
    packets->erase(packets->begin() + used_packets_id[i]);
  }

  return p;
}

Packet *
PacketAggregation::amsdu_deaggregation(Vector<Packet *> * /*packets*/, uint32_t /*max_size*/ , Vector<Packet *> * /*used_packets*/)
{
    return NULL;
}

Packet *
PacketAggregation::ampdu_aggregation(Vector<Packet *> * /*packets*/ , uint32_t /*max_size*/, Vector<Packet *> * /*used_packets*/)
{
    return NULL;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(PacketAggregation)
