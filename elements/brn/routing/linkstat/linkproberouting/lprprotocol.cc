#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/packet.hh>
#include <click/packet_anno.hh>
#include "lprprotocol.hh"
#include "elements/brn/standard/bitfield/bitfield.hh"
#include "elements/brn/standard/compression/lzw.hh"

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
  int bitLen, len, pindex,b;

  Bitfield bitfield(packet,p_len);

  bitsPerNode = calcNoBits(info->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  bitsPerLink = 4;

  bitLen = ( sizeof(struct packed_link_header) + (info->_header->_no_nodes * 6 ) ) * 8;  //header, macs
  bitLen += info->_header->_no_nodes * (bitsPerTS + bitsPerNode); //timestamp and stop marker for links

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    int links = 0;
    for ( int j = 0; j < info->_header->_no_nodes; j++ )
      if ( info->_links[i * info->_header->_no_nodes + j] != 0 ) links++;

    bitLen += links * (bitsPerNode + bitsPerLink);  //Src + link
  }

  len = (bitLen + 7) / 8;
  if ( len > p_len ) return -1;

  memcpy(packet, info->_header, sizeof(struct packed_link_header));
  pindex = sizeof(struct packed_link_header);
  memcpy(&(packet[pindex]), info->_macs, 6 * info->_header->_no_nodes );
  pindex += 6 * info->_header->_no_nodes;

  if ( pindex < len ) memset(&(packet[pindex]), 0, (len-pindex)); //clear the rest

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
  LZW lzw;
  int lzwsize = 0;

  Bitfield bitfield(packet,p_len);

  int bitsPerNode = calcNoBits(info->_header->_no_nodes);
  int invalidNode = (1 << bitsPerNode) - 1;
  int bitsPerTS = 8;
  //bitsPerLink = 4;
  int bitsPerDoubleLink = 7;

/*
  int bitLen = ( sizeof(struct packed_link_header) + (info->_header->_no_nodes * 6 ) ) * 8;  //header, macs
  bitLen += info->_header->_no_nodes * (bitsPerTS + bitsPerNode); //timestamp and stop marker for links

  int links = 0;
  int doubleLinks = 0;

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
*/
  memcpy(packet, info->_header, sizeof(struct packed_link_header));
  int pindex = sizeof(struct packed_link_header);

  if ( pindex < p_len ) memset(&(packet[pindex]), 0, (p_len-pindex)); //clear the rest

  if ( info->_header->_version == VERSION_BASE_MAC_LZW )
    lzwsize = lzw.encode( (unsigned char*)info->_macs, 6 * info->_header->_no_nodes, &(packet[pindex + 2]), (p_len-pindex)-2 );

  //TODO: reorder for mor performance
  if ( ( info->_header->_version == VERSION_BASE ) ||
         ( ( lzwsize + 2 ) >= 6 * info->_header->_no_nodes ) ) {
    memcpy(&(packet[pindex]), info->_macs, 6 * info->_header->_no_nodes );
    pindex += 6 * info->_header->_no_nodes;
    info->_header->_version = VERSION_BASE;
    memcpy(packet, info->_header, sizeof(struct packed_link_header));
  } else {
    packet[pindex] = lzwsize / 256;
    packet[pindex + 1] = lzwsize % 256;
    pindex += lzwsize + 2;
  }

  click_chatter("PackSize: %d %d",lzwsize,6 * info->_header->_no_nodes);

  pindex *= 8; //switch to bitarray

  for ( int b = 0; b < info->_header->_no_nodes ; b++) {
    bitfield.setValue(pindex, pindex + bitsPerTS - 1, info->_timestamp[b]);
    pindex += bitsPerTS;
  }

  for ( int i = 0; i < info->_header->_no_nodes; i++ ) {
    for ( int j = 0; j < info->_header->_no_nodes; j++ ) {
      if ( ( info->_links[i * info->_header->_no_nodes + j] != 0 ) && ( ( i < j ) || ( info->_links[j * info->_header->_no_nodes + i] == 0 ) ) ) {
        bitfield.setValue(pindex, pindex + bitsPerNode - 1, j);
        pindex += bitsPerNode;
        int overallLink = info->_links[i * info->_header->_no_nodes + j];
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
  int bitsPerNode, bitsPerTS, bitsPerDoubleLink; /*bitsPerLink */
  int invalidNode;
  int b;
  int pindex = 0;
  int ac_node, pair_node, pair_metric;

  Bitfield bitfield(packet,p_len);
  LZW lzw;

  memcpy(plh, packet, sizeof(struct packed_link_header));
  pli->_header = plh;
  pindex += sizeof(struct packed_link_header);

  bitsPerNode = calcNoBits(pli->_header->_no_nodes);
  invalidNode = (1 << bitsPerNode) - 1;
  bitsPerTS = 8;
  //bitsPerLink = 4;
  bitsPerDoubleLink = 7;

  pli->_macs = new struct etheraddr[pli->_header->_no_nodes];

  if ( pli->_header->_version == VERSION_BASE_MAC_LZW ) {
    int  lzwsizecomp = packet[pindex] * 256;
    lzwsizecomp += packet[pindex + 1];

    int lzwsize = lzw.decode(&(packet[pindex + 2]), lzwsizecomp, (unsigned char*)pli->_macs, 6*pli->_header->_no_nodes );
    pindex += lzwsizecomp + 2;
//    click_chatter("Size: %d %d",lzwsize,lzwsizecomp);
    assert( lzwsize == (6 * pli->_header->_no_nodes));
  } else {
    memcpy(pli->_macs, &(packet[pindex]), 6 * pli->_header->_no_nodes);
    pindex += 6 * pli->_header->_no_nodes;
  }

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
ELEMENT_PROVIDES(LPRProtocol)

