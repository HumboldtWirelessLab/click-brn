#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <click/standard/addressinfo.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "vaserver.hh"
#include "csi_protocol.h"

CLICK_DECLS

#define METRIC_ROUTING

VAServer::VAServer():
  _strategy(VIRTUAL_ANTENNA_STRATEGY_MAX_MIN_RSSI),
  _rtctrl(NULL),
  _rt_update_timer(this),
  _rt_update_interval(0),
  _verbose(false)
{
  BRNElement::init();
}

VAServer::~VAServer()
{
}

int
VAServer::configure(Vector<String> &conf, ErrorHandler* errh)
{
  if (cp_va_kparse(conf, this, errh,
      "RTCTRL", cpkP+cpkM, cpElement, &_rtctrl,
      "STRATEGY", cpkP, cpInteger, &_strategy,
      "INTERVAL", cpkP, cpInteger, &_rt_update_interval,
      "VERBOSE", cpkP, cpBool, &_verbose,
      "DEBUG", cpkP, cpInteger, &_debug,
      cpEnd) < 0)
    return -1;

  return 0;
}

int
VAServer::initialize(ErrorHandler *)
{
  _rt_update_timer.initialize(this);

  if (_rt_update_interval > 0 )
    _rt_update_timer.schedule_after_msec(_rt_update_interval);

  return 0;
}

void
VAServer::run_timer(Timer *)
{
  _rt_update_timer.schedule_after_msec(_rt_update_interval);

  for (VAClientMapIter iter = va_cl_info_map.begin(); iter.live(); iter++) {
    VAClientInfo *vaci = iter.value();

    IPAddress *best = select_ap(vaci->_ip);
    BRN_DEBUG("Best for %s is %s", vaci->_ip.unparse().c_str(), best->unparse().c_str());

    set_ap(vaci->_ip, *best);
  }

}

void
VAServer::push( int /*port*/, Packet *packet )
{
  struct csi_packet *csi_p = (struct csi_packet *)packet->data();
  IPAddress csi_node = IPAddress(csi_p->node.csi_node_addr);
  IPAddress tx_node = IPAddress(csi_p->node.tx_node_addr);

  if ( BRN_DEBUG_LEVEL_LOG ) {
    click_chatter("Eff. SNRS (%s -> %s):",tx_node.unparse().c_str(), csi_node.unparse().c_str());
    for ( int i = 0; i < MAX_NUM_RATES; i++) {
      click_chatter("%d %d %d %d", csi_p->eff_snrs_int[i][0], csi_p->eff_snrs_int[i][1],
                                   csi_p->eff_snrs_int[i][2], csi_p->eff_snrs_int[i][3]);
    }
  }

  if ( _verbose ) {
    click_chatter("CSI,%s,%s,%d,%d,%d,%d,%d,%d,%d", Timestamp::now().unparse().c_str(), csi_node.unparse().c_str(),
                                                    csi_p->eff_snrs_int[0][0], csi_p->eff_snrs_int[1][0],
                                                    csi_p->eff_snrs_int[2][0], csi_p->eff_snrs_int[3][0],
                                                    csi_p->eff_snrs_int[4][0], csi_p->eff_snrs_int[7][0],
                                                    csi_p->eff_snrs_int[6][0]);
  }

  add_csi(csi_node, tx_node, csi_p->eff_snrs_int);

  if ( _rt_update_interval == 0 ) {
    IPAddress *best = select_ap(tx_node);
    BRN_DEBUG("Best for %s is %s",tx_node.unparse().c_str(), best->unparse().c_str());

    set_ap(tx_node, *best);
  }

  packet->kill();
}

void
VAServer::add_csi(IPAddress csi_node, IPAddress client, uint32_t eff_snrs_int[MAX_NUM_RATES][4])
{
  VAAntennaInfo **vaai_p = va_ant_info_map.findp(csi_node);
  VAAntennaInfo *vaai = NULL;

  VAClientInfo **vaci_p = va_cl_info_map.findp(client);
  VAClientInfo *vaci = NULL;

  bool client_is_new = false;

  if (vaai_p == NULL) {
    vaai = new VAAntennaInfo(csi_node);
    va_ant_info_map.insert(csi_node, vaai);
  } else {
    vaai = *vaai_p;
  }

  if (vaci_p == NULL) {
    IPAddress mask = IPAddress::make_broadcast();
    vaci = new VAClientInfo(client);
    vaci->set_mask(mask);
    va_cl_info_map.insert(client, vaci);
    client_is_new = true;
  } else {
    vaci = *vaci_p;
  }

  CSI **csi_p = vaci->csi_map.findp(csi_node);
  CSI *csi = NULL;

  if (csi_p == NULL) {
    csi = new CSI(client, csi_node, eff_snrs_int);
    vaci->csi_map.insert(csi_node, csi);
#ifdef METRIC_ROUTING
    if ( client_is_new ) {
      vaci->set_gateway(vaai);
      LinuxRTCtrl::set_rt_entry_metric(&(vaci->routing_entry),0);
      _rtctrl->add_rt_entry(&(vaci->routing_entry));
    } else {
      LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(vaci->routing_entry.rt_gateway), csi_node);
      LinuxRTCtrl::set_rt_entry_metric(&(vaci->routing_entry),1000);
      _rtctrl->add_rt_entry(&(vaci->routing_entry));

      LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(vaci->routing_entry.rt_gateway), vaci->current_antenna->_ip);
      LinuxRTCtrl::set_rt_entry_metric(&(vaci->routing_entry),0);
    }
#endif
  } else {
    csi = *csi_p;
    csi->update_csi(eff_snrs_int);
  }
}

IPAddress*
VAServer::select_ap(IPAddress client)
{
  VAClientInfo **vaci_p = va_cl_info_map.findp(client);
  VAClientInfo *vaci = NULL;

  CSI *best_csi = NULL;

  if (vaci_p == NULL) {
    BRN_ERROR("Client %s not found");
    return NULL;
  }

  vaci = *vaci_p;

  uint32_t timenow_ms = Timestamp::now().msecval();

  switch (_strategy) {
    case VIRTUAL_ANTENNA_STRATEGY_MAX_MIN_RSSI: {
      uint32_t min_snr = 0;
      uint32_t max_min_snr_overall = 0;

      for (CSIMapIter iter = vaci->csi_map.begin(); iter.live(); iter++) {
        CSI *csi = iter.value();

        //for ( int i = 0; i < MAX_NUM_RATES; i++) {
          int i = FIRST_MIMO3;

          min_snr = 0;

          for ( int s = 0; s < 4; s++) {
            if ( (csi->_eff_snrs[i][s] < min_snr) ||  (min_snr == 0))
              min_snr = csi->_eff_snrs[i][s];
          }

          if (min_snr > max_min_snr_overall) {
            best_csi = csi;
            max_min_snr_overall = min_snr;
          }
        //}
      }

      break;
    }
    case VIRTUAL_ANTENNA_STRATEGY_MAX_THROUGHPUT: {
      //
      // search the AP which offers the highest throughput

      uint32_t max_data_rate_all = 0; // best data rate among all APs

      for (CSIMapIter iter = vaci->csi_map.begin(); iter.live(); iter++) { // for each available AP
        CSI *csi = iter.value();

        CSI *best_csi_ap = NULL;
        uint32_t max_data_rate = 0; // best data rate for a given AP

        for ( int i = 0; i < MAX_NUM_RATES; i++) { // for each available MODE: 3x SISO, 2x MIMO_2, 1x MIMO_3

          // calc spatial multiplexing gain
          int sm_gain = sm_gain_lookup_table[i];

          for ( int t = 0; t < 8; t++) { // for each available MCS
            // eff. SNR depends on the MCS; select the proper bin
            int effSnrBin = eff_snr_bin_lookup_table[t];

            if (csi->_eff_snrs[i][effSnrBin] > mcs_to_snr_threshold[t]) { // the eff. SNR is larger than the threshold

              // calc data rate
              uint32_t data_rate = mcs_to_data_rate_kbps[t] * sm_gain; // we have parallel streams

              // valid MCS (above threshold)
              if (data_rate > max_data_rate_all) { // total best
                best_csi = csi;
                max_data_rate_all = data_rate;
              }
              if (data_rate > max_data_rate) { // best solution for each AP
                best_csi_ap = csi;
                max_data_rate = data_rate;
              }
            }
          }
        }

        // print best solution for a given AP
        if ( BRN_DEBUG_LEVEL_DEBUG && (best_csi_ap != NULL) ) {
          click_chatter("%d max data rate towards AP %s is %d", timenow_ms, best_csi_ap->_to.unparse().c_str(), max_data_rate);
        }

      }
      break;
    }
    default: {
      BRN_ERROR("Unknown strategy!");
    }
  }

  if ( best_csi == NULL ) return NULL;

  return &(best_csi->_to);
}

void
VAServer::set_ap(IPAddress &client, IPAddress &ap)
{
  VAClientInfo **vaci_p = va_cl_info_map.findp(client);
  VAClientInfo *vaci = NULL;

  if (vaci_p == NULL) {
    BRN_ERROR("Client %s not found");
    return;
  }

  vaci = *vaci_p;

  //We already set this ap as va?
  if ( vaci->current_antenna->_ip == ap ) return;

  VAAntennaInfo **vaai_p = va_ant_info_map.findp(ap);
  VAAntennaInfo *vaai = NULL;

  if (vaai_p == NULL) {
    BRN_ERROR("AP %s not found");
    return;
  }

  vaai = *vaai_p;

  struct rtentry *routing_entry = &(vaci->routing_entry);

#ifdef METRIC_ROUTING
  LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry->rt_gateway), ap); //set new ap (temporarily)
  _rtctrl->add_rt_entry(routing_entry);

  LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry->rt_gateway), vaci->current_antenna->_ip); //set old ap (temporarily) (just for del)
  _rtctrl->del_rt_entry(routing_entry);
  LinuxRTCtrl::set_rt_entry_metric(routing_entry,1000);
  _rtctrl->add_rt_entry(routing_entry);

  LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry->rt_gateway), ap); //set new ap (temporarily)
  _rtctrl->del_rt_entry(routing_entry);

  LinuxRTCtrl::set_rt_entry_metric(routing_entry,0);
#else
  LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry->rt_gateway), ap); //set new ap (temporarily)
  _rtctrl->add_rt_entry(routing_entry);
  LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry->rt_gateway), vaci->current_antenna->_ip); //set old ap (temporarily) (just for del)
  _rtctrl->del_rt_entry(routing_entry);
#endif

  vaci->set_gateway(vaai); //set final

  if ( _verbose ) {
    click_chatter("RT,%s,%s,%s,0,0,0,0,0,0", Timestamp::now().unparse().c_str(), vaci->_ip.unparse().c_str(),
                                                    vaai->_ip.unparse().c_str());
  }

}

String
VAServer::stats()
{
  StringAccum sa;
  sa << "<vaserver node=\"" << BRN_NODE_NAME << "\" >\n";
  sa << "\t<aps count=\"" << va_ant_info_map.size() << "\">\n";

  for (VAInfoMapIter iter = va_ant_info_map.begin(); iter.live(); iter++) {
    VAAntennaInfo *vai = iter.value();
    sa << "\t\t<ap addr=\"" << vai->_ip.unparse() << "\" />\n";
  }

  sa << "\t</aps>\n\t<clients count=\"" << va_cl_info_map.size() << "\">\n";

  for (VAClientMapIter iter = va_cl_info_map.begin(); iter.live(); iter++) {
    VAClientInfo *vaci = iter.value();
    sa << "\t\t<client addr=\"" << vaci->_ip.unparse() << "\" >\n";

    for (CSIMapIter iter = vaci->csi_map.begin(); iter.live(); iter++) {
      CSI *csi = iter.value();
      sa << "\t\t\t<ap addr=\"" << csi->_to.unparse() << "\" >\n";

      for ( int i = 0; i < MAX_NUM_RATES; i++) {
        if (i < FIRST_MIMO2) sa << "\t\t\t\t<siso idx=\"" << i;
        else if (i < FIRST_MIMO3) sa << "\t\t\t\t<mimo2 idx=\"" << (i-FIRST_MIMO2);
        else sa << "\t\t\t\t<mimo3 idx=\"" << (i-FIRST_MIMO3);

        sa << "\" snr0=\"" << csi->_eff_snrs[i][0] << "\" snr1=\"" << csi->_eff_snrs[i][1];
        sa << "\" snr2=\"" << csi->_eff_snrs[i][2] << "\" snr3=\"" << csi->_eff_snrs[i][3] << " />\n";
      }

      sa << "\t\t\t</ap>\n";
    }

    sa << "\t\t</client>\n";
  }

  sa << "\t</clients>\n</vaserver>\n";

  return sa.take_string();
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------
enum {H_STATS};

static String
VAServer_read_param(Element *e, void *thunk)
{
  VAServer *vas = (VAServer *)e;
  switch ((uintptr_t) thunk) {
    case H_STATS: return vas->stats();
  }
  return String();
}

void
VAServer::add_handlers()
{
  BRNElement::add_handlers();

  add_read_handler("stats", VAServer_read_param, (void *) H_STATS);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(VAServer)
