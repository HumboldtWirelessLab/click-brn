#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "lprprotocol.hh"
#include "elements/brn2/standard/bitfield/bitfield.hh"


CLICK_DECLS

LPRProtocol::LPRProtocol()
{
}

LPRProtocol::~LPRProtocol()
{
}

int
calcNoBits(int nodes)
{  //use loop to avoid doubles,...
  for(int i = 0; i <= 16; i++ )
    if ( ( 1 << i ) > nodes ) return i;

  return -1;
}

int
LPRProtocol::pack(struct packed_link_info *info, unsigned char *packet, int p_len)
{
  int bitsPerNode, bitsPerTS, bitsPerLink;
  int invalidNode;
  int bitLen,len,links, pindex,b;

  Bitfield bitfield(packet,p_len);

  bitsPerNode = calcNoBits(info->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  bitsPerLink = 4;

  bitLen = ( sizeof(struct packed_link_header) + (info->_header->_no_nodes * 6 ) ) * 8;  //header, macs
  bitLen += info->_header->_no_nodes * (bitsPerTS + bitsPerNode); //timestamp and stop marker for links

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    links = 0;
    for ( int j = 0; j < info->_header->_no_nodes; j++ )
      if ( info->_links[i * info->_header->_no_nodes + j] != 0 ) links++;

    bitLen += links * (bitsPerNode + bitsPerLink);  //Src + link
  }

  len = (bitLen + 7) / 8;
  if ( len > p_len ) return -1;

  memset(packet,0,len);
  memcpy(packet, info->_header, sizeof(struct packed_link_header));
  pindex = sizeof(struct packed_link_header);
  memcpy(&(packet[pindex]), info->_macs, 6 * info->_header->_no_nodes );
  pindex += 6 * info->_header->_no_nodes;

  pindex *= 8; //switch to bitarray

  for ( b = 0; b < info->_header->_no_nodes ; b++) {
    bitfield.setValue(pindex, pindex + bitsPerTS - 1, info->_timestamp[b]);
    pindex += bitsPerTS;
  }

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    for ( int j = 0; j < info->_header->_no_nodes; j++ ) {
      if ( info->_links[i * info->_header->_no_nodes + j] != 0 ) {
        bitfield.setValue(pindex, pindex + bitsPerNode - 1, j);
        pindex += bitsPerNode;
        bitfield.setValue(pindex, pindex + bitsPerLink - 1, info->_links[i * info->_header->_no_nodes + j]);
        pindex += bitsPerLink;
      }
    }
    bitfield.setValue(pindex, pindex + bitsPerNode - 1, invalidNode );
    pindex += bitsPerNode;
  }

  return len;
}

struct packed_link_info *
LPRProtocol::unpack(unsigned char *packet, int p_len) {
  struct packed_link_header *plh = new struct packed_link_header;
  struct packed_link_info *pli = new struct packed_link_info;
  int bitsPerNode, bitsPerTS, bitsPerLink;
  int invalidNode;
  int b;
  int pindex = 0;
  int ac_node, pair_node, pair_metric;
  Bitfield bitfield(packet,p_len);

  memcpy(plh, packet, sizeof(struct packed_link_header));
  pli->_header = plh;
  pindex += sizeof(struct packed_link_header);

  bitsPerNode = calcNoBits(pli->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  bitsPerLink = 4;

  pli->_macs = new struct etheraddr[pli->_header->_no_nodes];
  memcpy(pli->_macs, &(packet[pindex]), 6 * pli->_header->_no_nodes);
  pindex += 6 * pli->_header->_no_nodes;

  pindex *= 8; //switch to bitarray

  pli->_timestamp = new unsigned char[pli->_header->_no_nodes];
  memset(pli->_timestamp,0,pli->_header->_no_nodes);

  for ( b = 0; b < pli->_header->_no_nodes ; b++) {
    pli->_timestamp[b] = bitfield.getValue(pindex, pindex + bitsPerTS - 1);
    pindex += bitsPerTS;
  }

  pli->_links = new unsigned char[pli->_header->_no_nodes * pli->_header->_no_nodes];
  memset(pli->_links, 0, pli->_header->_no_nodes * pli->_header->_no_nodes);

  for ( ac_node = 0; ac_node < pli->_header->_no_nodes; ac_node++ ) {
    while ( bitfield.getValue(pindex, pindex + bitsPerNode - 1) != invalidNode ) {
      pair_node = bitfield.getValue(pindex, pindex + bitsPerNode - 1);
      pindex+=bitsPerNode;
      pair_metric = bitfield.getValue(pindex, pindex + bitsPerLink - 1);
      pindex+=bitsPerLink;
      pli->_links[ac_node * pli->_header->_no_nodes + pair_node] = (unsigned char)pair_metric;
    }
    pindex+=bitsPerNode;
  }

  return pli;
}

int
LPRProtocol::pack2(struct packed_link_info *info, unsigned char *packet, int p_len)
{
  int bitsPerNode, bitsPerTS, bitsPerLink, bitsPerDoubleLink;
  int invalidNode;
  int bitLen,len,links, doubleLinks, pindex,b;
  int overallLink;

  Bitfield bitfield(packet,p_len);

  bitsPerNode = calcNoBits(info->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  bitsPerLink = 4;
  bitsPerDoubleLink = 7;

  bitLen = ( sizeof(struct packed_link_header) + (info->_header->_no_nodes * 6 ) ) * 8;  //header, macs
  bitLen += info->_header->_no_nodes * (bitsPerTS + bitsPerNode); //timestamp and stop marker for links

  links = 0;
  doubleLinks = 0;

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    for ( int j = 0; j < info->_header->_no_nodes; j++ ) {
      if ( ( info->_links[i * info->_header->_no_nodes + j] != 0 ) &&
           ( info->_links[j * info->_header->_no_nodes + i] != 0 ) ) doubleLinks++;
      else
        if ( info->_links[i * info->_header->_no_nodes + j] != 0 ) links++;
    }
  }

  bitLen += links * (bitsPerNode + bitsPerDoubleLink);  //Src + link
  bitLen += (doubleLinks / 2) * (bitsPerNode + bitsPerDoubleLink);  //Src + link

  len = (bitLen + 7) / 8;
  if ( len > p_len ) return -1;

  memset(packet,0,len);
  memcpy(packet, info->_header, sizeof(struct packed_link_header));
  pindex = sizeof(struct packed_link_header);
  memcpy(&(packet[pindex]), info->_macs, 6 * info->_header->_no_nodes );
  pindex += 6 * info->_header->_no_nodes;

  pindex *= 8; //switch to bitarray

  for ( b = 0; b < info->_header->_no_nodes ; b++) {
    bitfield.setValue(pindex, pindex + bitsPerTS - 1, info->_timestamp[b]);
    pindex += bitsPerTS;
  }

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    for ( int j = 0; j < info->_header->_no_nodes; j++ ) {
      if ( ( info->_links[i * info->_header->_no_nodes + j] != 0 ) && ( ( i < j ) || ( info->_links[j * info->_header->_no_nodes + i] == 0 ) ) ) {
        bitfield.setValue(pindex, pindex + bitsPerNode - 1, j);
        pindex += bitsPerNode;
        overallLink = info->_links[i * info->_header->_no_nodes + j];
        overallLink = ( 11 * overallLink ) + (int)(info->_links[j * info->_header->_no_nodes + i]);
        bitfield.setValue(pindex, pindex + bitsPerDoubleLink - 1, overallLink);
        pindex += bitsPerDoubleLink;
      }
    }
    bitfield.setValue(pindex, pindex + bitsPerNode - 1, invalidNode );
    pindex += bitsPerNode;
  }

  return (pindex + 7) / 8;
}

struct packed_link_info *
LPRProtocol::unpack2(unsigned char *packet, int p_len) {
  struct packed_link_header *plh = new struct packed_link_header;
  struct packed_link_info *pli = new struct packed_link_info;
  int bitsPerNode, bitsPerTS, bitsPerLink, bitsPerDoubleLink;
  int invalidNode;
  int b;
  int pindex = 0;
  int ac_node, pair_node, pair_metric;

  Bitfield bitfield(packet,p_len);

  memcpy(plh, packet, sizeof(struct packed_link_header));
  pli->_header = plh;
  pindex += sizeof(struct packed_link_header);

  bitsPerNode = calcNoBits(pli->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  bitsPerLink = 4;
  bitsPerDoubleLink = 7;

  pli->_macs = new struct etheraddr[pli->_header->_no_nodes];
  memcpy(pli->_macs, &(packet[pindex]), 6 * pli->_header->_no_nodes);
  pindex += 6 * pli->_header->_no_nodes;

  pindex *= 8; //switch to bitarray

  pli->_timestamp = new unsigned char[pli->_header->_no_nodes];
  memset(pli->_timestamp,0,pli->_header->_no_nodes);

  for ( b = 0; b < pli->_header->_no_nodes ; b++) {
    pli->_timestamp[b] = bitfield.getValue(pindex, pindex + bitsPerTS - 1);
    pindex += bitsPerTS;
  }

  pli->_links = new unsigned char[pli->_header->_no_nodes * pli->_header->_no_nodes];
  memset(pli->_links, 0, pli->_header->_no_nodes * pli->_header->_no_nodes);

  for ( ac_node = 0; ac_node < pli->_header->_no_nodes; ac_node++ ) {
    while ( bitfield.getValue(pindex, pindex + bitsPerNode - 1) != invalidNode ) {
      pair_node = bitfield.getValue(pindex, pindex + bitsPerNode - 1);
      pindex+=bitsPerNode;
      pair_metric = bitfield.getValue(pindex, pindex + bitsPerDoubleLink - 1);
      pindex+=bitsPerDoubleLink;
      pli->_links[ac_node * pli->_header->_no_nodes + pair_node] = (unsigned char)pair_metric / 11;
      pli->_links[pair_node * pli->_header->_no_nodes + ac_node] = (unsigned char)pair_metric % 11;
    }
    pindex+=bitsPerNode;
  }

  return pli;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(LPRProtocol)

