#include <click/config.h>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>

#include <elements/brn/standard/brnaddressinfo.hh>
#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "vaserver.hh"
#include "csi_protocol.h"

CLICK_DECLS

VAServer::VAServer():
  _strategy(VIRTUEL_ANTENNA_STRATEGY_MAX_MIN_RSSI),
  _rtctrl(NULL),
  _rt_update_timer(this),
  _rt_update_interval(0)
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

  click_chatter("Eff. SNRS (%s -> %s):",tx_node.unparse().c_str(), csi_node.unparse().c_str());
  for ( int i = 0; i < MAX_NUM_RATES; i++) {
    click_chatter("%d %d %d %d", csi_p->eff_snrs_int[i][0], csi_p->eff_snrs_int[i][1],
                                 csi_p->eff_snrs_int[i][2], csi_p->eff_snrs_int[i][3]);
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
  } else {
    vaci = *vaci_p;
  }

  CSI **csi_p = vaci->csi_map.findp(csi_node);
  CSI *csi = NULL;

  if (csi_p == NULL) {
    csi = new CSI(client, csi_node, eff_snrs_int);
    vaci->csi_map.insert(csi_node, csi);
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

  switch (_strategy) {
    case VIRTUEL_ANTENNA_STRATEGY_MAX_MIN_RSSI: {
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

      if ( best_csi == NULL ) return NULL;
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

  VAAntennaInfo **vaai_p = va_ant_info_map.findp(ap);
  VAAntennaInfo *vaai = NULL;

  if (vaci_p == NULL) {
    BRN_ERROR("Client %s not found");
    return;
  }

  if (vaai_p == NULL) {
    BRN_ERROR("AP %s not found");
    return;
  }

  vaci = *vaci_p;
  vaai = *vaai_p;

  _rtctrl->del_rt_entry(&(vaci->routing_entry));

  vaci->set_gateway(vaai);

  _rtctrl->add_rt_entry(&(vaci->routing_entry));
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
