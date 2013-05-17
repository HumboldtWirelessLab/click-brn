#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/packet.hh>
#include <click/timestamp.hh>

#include "elements/brn/wifi/brnwifi.hh"
#include "elements/brn/brnelement.hh"
#include "elements/wifi/bitrate.hh"
#include "analyzephantom.hh"


CLICK_DECLS


AnalyzePhantom::AnalyzePhantom():
  last_packet(NULL),
  delay_timer(this),
  delay(DEFAULT_DELAY)
{
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


int AnalyzePhantom::initialize(ErrorHandler *)
{
  delay_timer.initialize(this);
  return 0;
}


void AnalyzePhantom::run_timer(Timer *)
{
  send_last_packet();
}


Packet *
AnalyzePhantom::simple_action(Packet *p)
{
  struct click_wifi_extra *ceha = WIFI_EXTRA_ANNO(p);

  if (ceha->flags & WIFI_EXTRA_TX) {
    send_last_packet();
    return p;
  }

  if (last_packet == NULL) {
    delay_packet(p);
    return NULL;
  }

  delay_timer.unschedule();

  if (analyze_new(p) == NEW_LAST_PACKET)
    return NULL;
  else /* reset */
    return p;
}

void AnalyzePhantom::delay_packet(Packet *p) {
  last_packet = p;
  analyze_last();
  delay_timer.reschedule_after_msec(delay);
}


void
AnalyzePhantom::send_last_packet()
{
  if (last_packet == NULL)
    return;

  output(0).push(last_packet);
  last_packet = NULL;
}


void
AnalyzePhantom::analyze_last()
{
  struct click_wifi_extra *cwe_last = WIFI_EXTRA_ANNO(last_packet);

  if (last_packet == NULL) {
    BRN_DEBUG("AnalyzePhantom::analyze_last: last_packet was NULL");
    return;
  }

  /* single phantom */
  if ((cwe_last->flags & WIFI_EXTRA_RX_PHANTOM_ERR) == WIFI_EXTRA_RX_PHANTOM_ERR) {
    cwe_last->flags = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_INRANGE | WIFI_EXTRA_RX_NOWIFI;

  /* single crc */
  } else if ((cwe_last->flags & WIFI_EXTRA_RX_CRC_ERR) == (uint32_t)WIFI_EXTRA_RX_CRC_ERR) {
    cwe_last->flags = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_INRANGE | WIFI_EXTRA_RX_HN | WIFI_EXTRA_RX_NOWIFI;
  }
}


int
AnalyzePhantom::analyze_new(Packet *p)
{
  /* no pkt, shouldn't have been called */
  if ((p == NULL) && (last_packet == NULL)) {
    BRN_DEBUG("AnalyzePhantom.analyze() - ERROR: Packet Pointer was null!");
    return RESET;
  }

  struct click_wifi_extra *cwe_last = WIFI_EXTRA_ANNO(last_packet);
  struct click_wifi_extra *cwe_new  = WIFI_EXTRA_ANNO(p);

  /* check for CRC directly followed by Phantom */
  if (((cwe_last->flags & WIFI_EXTRA_RX_CRC_ERR) == (uint32_t)WIFI_EXTRA_RX_CRC_ERR) &&
      ((cwe_new->flags & WIFI_EXTRA_RX_PHANTOM_ERR) == WIFI_EXTRA_RX_PHANTOM_ERR)) {

      u_int64_t packet_gap = get_packet_gap_via_timestamp(p);

      if (packet_gap <= MAX_GAP) {
        cwe_last->flags = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_INRANGE | WIFI_EXTRA_RX_HN | WIFI_EXTRA_RX_NOWIFI;
        cwe_new->flags  = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_INRANGE | WIFI_EXTRA_RX_HN | WIFI_EXTRA_RX_NOWIFI;
        send_last_packet();

      } else { /* single crc followed by single phantom*/
        send_last_packet();
        delay_packet(p);
        return NEW_LAST_PACKET;
      }

      return RESET;
  }

  /* check for Phantom followed by CRC */
  if (((cwe_last->flags & WIFI_EXTRA_RX_PHANTOM_ERR) == WIFI_EXTRA_RX_PHANTOM_ERR) &&
      ((cwe_new->flags & WIFI_EXTRA_RX_CRC_ERR) == (uint32_t)WIFI_EXTRA_RX_CRC_ERR)) {

      struct add_phantom_data * ph_data = (struct add_phantom_data *) last_packet->data();
      u_int64_t packet_gap = get_packet_gap_via_timestamp(p);

      if ((ph_data->next_state == STATE_RX) && (packet_gap <= MAX_GAP)) {
        cwe_last->flags = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_NOWIFI;
        cwe_new->flags  = WIFI_EXTRA_RX_ACI | WIFI_EXTRA_RX_NOWIFI;
        send_last_packet();

      } else { /* single phantom followed by single crc */
        send_last_packet();
        delay_packet(p);
        return NEW_LAST_PACKET;
      }

    return RESET;
  }


  /* check for PHY directly followed by RX-OK */
  if (((cwe_last->flags & WIFI_EXTRA_RX_PHY_ERR) ==  WIFI_EXTRA_RX_PHY_ERR) && (cwe_new->flags == RX_OK)) {
    u_int64_t packet_gap = get_packet_gap_via_hosttime(p);

    if (packet_gap <= MAX_GAP) {
      cwe_last->flags = WIFI_EXTRA_RX_HN;
      cwe_new->flags  = WIFI_EXTRA_RX_HN;
      send_last_packet();

    } else { /* single PHY followed by single RX */
      send_last_packet();
    }

    return RESET;
  }

  return RESET;
}


/* returns packet gap in usec, based on the packet anno timestamp */
u_int64_t
AnalyzePhantom::get_packet_gap_via_timestamp(Packet *p)
{
  struct click_wifi_extra *cwe_new  = WIFI_EXTRA_ANNO(p);

  u_int32_t len_2nd;        /* in bytes */
  u_int32_t duration_2nd;   /* in us */

  u_int64_t stop_1st;
  u_int64_t start_2nd;

  if (last_packet->timestamp_anno().usecval() == p->timestamp_anno().usecval()) {
    BRN_DEBUG("Error @ AnalyzePhantom::get_packet_gap_via_timestamp:\n");
    BRN_DEBUG("Both packets ended at the same time\n");
    return 0;
  }

  len_2nd  = p->length();

  /* duration in usec */
  duration_2nd = calc_transmit_time(cwe_new->rate, len_2nd);

  stop_1st  = last_packet->timestamp_anno().usecval();
  start_2nd = p->timestamp_anno().usecval() - (duration_2nd);

  return start_2nd - stop_1st;
}


/* returns packet gap in usec, based on the k_time from the kernel/driver */
u_int64_t
AnalyzePhantom::get_packet_gap_via_hosttime(Packet *p)
{
  struct click_wifi_extra *cwe_last = WIFI_EXTRA_ANNO(last_packet);
  struct click_wifi_extra *cwe_new  = WIFI_EXTRA_ANNO(p);

  u_int64_t stop_1st = BrnWifi::get_host_time(cwe_last);
  u_int64_t stop_2nd = BrnWifi::get_host_time(cwe_new);

  if (stop_1st == stop_2nd) {
    BRN_DEBUG("Error @ AnalyzePhantom::get_packet_via_hosttime:\n");
    BRN_DEBUG("Both packets ended at the same time\n");
    return 0;
  }

  /* duration is in usec, hosttime in nsec */
  u_int64_t duration_2nd = calc_transmit_time(cwe_new->rate, p->length());
  u_int64_t start_2nd    = stop_2nd - (duration_2nd * 1000);

  return start_2nd - stop_1st;
}



CLICK_ENDDECLS


EXPORT_ELEMENT(AnalyzePhantom)
