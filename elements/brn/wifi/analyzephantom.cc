#include "analyzephantom.hh"


CLICK_DECLS


AnalyzePhantom::AnalyzePhantom()
{
  int i;

  for (i = 0; i < QUEUE_LEN; i++) {

    pkt_q[i].p        = NULL;
    pkt_q[i].err_type = INIT;
    pkt_q[i].ts       = INIT;
  }


  BRNElement::init();
}


AnalyzePhantom::~AnalyzePhantom()
{
}


int
AnalyzePhantom::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret = cp_va_kparse(conf, this, errh,
                     "DEBUG", cpkP, cpInteger, &_debug,
                     cpEnd);

  return ret;
}


Packet *
AnalyzePhantom::simple_action(Packet *p)
{
  //BRN_DEBUG("Analyze Phantom");

  struct click_wifi_extra *ceha = WIFI_EXTRA_ANNO(p);
  struct pkt_q_entry pkt;


  if ( (ceha->flags & WIFI_EXTRA_RX_PHANTOM_ERR) == WIFI_EXTRA_RX_PHANTOM_ERR ) {
    pkt.p        = p;
    pkt.err_type = WIFI_EXTRA_RX_PHANTOM_ERR;
    pkt.ts       = timestamp_anno():

    //insert(pkt);
    analyze();

  } else if ( (ceha->flags & WIFI_EXTRA_RX_CRC_ERR) == WIFI_EXTRA_RX_CRC_ERR ) {
    pkt.p        = p;
    pkt.err_type = WIFI_EXTRA_RX_CRC_ERR;
    pkt.ts       = timestamp_anno():

    //insert(pkt);
    analyze();

  } else if ( (ceha->flags & WIFI_EXTRA_RX_PHY_ERR) ==  WIFI_EXTRA_RX_PHY_ERR ) {
    pkt.p        = p;
    pkt.err_type = WIFI_EXTRA_RX_PHY_ERR;
    pkt.ts       = timestamp_anno():

    //insert(pkt);
    analyze();

  } else {
    /* TODO? */
  }

  BRN_DEBUG("%s\n", pkt.ts.unparse());

  return p;
}


void
AnalyzePhantom::insert_pkt(Paket *p, u_int32_t err_type)
{
  int i;

  /*
   * if the queue length isn't trivial, shift all queue entries and
   * insert the new packet at the head position
   */
  if (QUEUE_LEN > 1) {
    for (i = QUEUE_LEN-1; i > 0; i--) {
      pkt_q[i] = pkt_q[i-1];
    }
  }

}


void
AnalyzePhantom::analyze()
{

}



CLICK_ENDDECLS


EXPORT_ELEMENT(AnalyzePhantom)
