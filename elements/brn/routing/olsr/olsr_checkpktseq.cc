
#include <click/config.h>
#include <click/confparse.hh>
#include <click/package.hh>
#include "olsr_packethandle.hh"
#include "olsr_checkpktseq.hh"
#include "click_olsr.hh"

CLICK_DECLS


OLSRCheckPacketSeq::OLSRCheckPacketSeq()
: _duplicateSet(NULL){
}


OLSRCheckPacketSeq::~OLSRCheckPacketSeq()
{
}


int
OLSRCheckPacketSeq::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_kparse(conf, this, errh,
      "Duplicate Set Element", cpkP, cpElement, &_duplicateSet,
		  cpEnd) < 0)
    return -1;
  return 0;
}


void
OLSRCheckPacketSeq::push(int, Packet *packet)
{
  pkt_hdr_info pkt_info = OLSRPacketHandle::get_pkt_hdr_info(packet);
  IPAddress source_addr = packet->dst_ip_anno();

  if ( pkt_seq_old(source_addr, pkt_info.pkt_seq) ){ 
    click_chatter("Dropping packet, pkt seq old");
    output(1).push(packet); //discard if packet sequence number is old
    return;
  }
  output(0).push(packet);
}


bool
OLSRCheckPacketSeq::pkt_seq_old(IPAddress source_addr, int pkt_seq)
{
  int prev_pkt_seq = 0;
  prev_pkt_seq = _duplicateSet->find_packet_seq(source_addr);
 
  if (prev_pkt_seq == 0){
    _duplicateSet->add_packet_seq(source_addr, pkt_seq);
    return false;
  }
  else{
    // check if new pkt seq is bigger than that allready stored according to RFC ch 19
    if (   ( pkt_seq > prev_pkt_seq && (pkt_seq - prev_pkt_seq) <= (OLSR_MAX_SEQNUM/2) )
	   ||  ( prev_pkt_seq > pkt_seq && (prev_pkt_seq - pkt_seq) > (OLSR_MAX_SEQNUM/2) )   ){    
      _duplicateSet->update_packet_seq(source_addr, pkt_seq);
      return false;
    }
  }
  return true;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(OLSRCheckPacketSeq);

