/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

/*
 * stationhandover.{cc,hh} -- measures rssi values of nearby APs and initiates
 *    reassociations
 */

#include <click/config.h>
#include "elements/brn/common.hh"
#include "stationhandover.hh"
#include <clicknet/wifi.h>
#include <elements/wifi/wirelessinfo.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/sync.hh>
#include <click/glue.hh>
#include <click/error.hh>
CLICK_DECLS

StationHandover::StationHandover() :
  _debug(BrnLogger::DEFAULT),
  _active( true ),
  _station_assoc(),
  _winfo()
{
  _ap_map = new AccessPointMap();
}

StationHandover::~StationHandover()
{
  delete _ap_map;
}

int
StationHandover::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
      cpElement, "BRNAssocRequester", &_station_assoc,
      cpElement, "WirelessInfo", &_winfo,
      cpInteger, "use #number of beacons to calculate the average rssi value", &_delta,
      cpInteger, "min rssi (dBm) difference for handover to avoid oscillations", &_rssi_min_diff,
      cpEnd) < 0)
    return -1;

  if (!_station_assoc || _station_assoc->cast("BRNAssocRequester") == 0) 
    return errh->error("BRNAssocRequester element is not provided or not a BRNAssocRequester");

  if (!_winfo || _winfo->cast("WirelessInfo") == 0) 
    return errh->error("WirelessInfo element is not provided or not a WirelessInfo");

  if (_delta < 0)
    return errh->error("_delta must be a positive number.");

  if (_rssi_min_diff < 0)
    return errh->error("_rssi_min_diff must be a positive number between 0 and 35.");

  return 0;
}

int
StationHandover::initialize(ErrorHandler *)
{
  return 0;
}

/**
  * Estimate and store the rssi value and the BSSID of an incoming 802.11 beacon.
  * If the rssi value of the current associated AP differs more than _rssi_min_diff
  * dBm from the best available AP than a reassociation process is started.
  */
void
StationHandover::push(int, Packet *p_in)
{
  if( false == _active )
    return;

  struct click_wifi_extra *ceha = (struct click_wifi_extra *) p_in->all_user_anno();
  struct click_wifi *wh = (struct click_wifi *) p_in->data();

  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
  int subtype = wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;

  switch (type) {
    // only mgmt frames are of interest
    case WIFI_FC0_TYPE_MGT:
      switch (subtype) {
        case WIFI_FC0_SUBTYPE_BEACON: {
          EtherAddress bssid = retrieve_bssid(p_in, _winfo->_ssid);
          BRN_DEBUG(" received beacon from %s with rssi %d.", bssid.s().c_str(), ceha->rssi);

          RSSIVector *rssi_vec = _ap_map->findp(bssid);

          if (!rssi_vec) {
            _ap_map->insert(bssid, RSSIVector());
            BRN_DEBUG("* _ap_map: new entry added: %s, %d", bssid.s().c_str(), ceha->rssi);
            rssi_vec = _ap_map->findp(bssid);
          } else {
            // check the number of entries
            if (rssi_vec->size() >= _delta) {
              BRN_DEBUG(" drop oldest entry");
              // FIFO dropping
              rssi_vec->erase(rssi_vec->begin());
            }
            rssi_vec->push_back(ceha->rssi);

            // check whether we have to make a handover
            check_for_handover();
          }
        }
      }
  }

  output(0).push(p_in);
}

/**
  * Calculates the average rssi for each neighboring AP. Process a handover if a better AP becomes available.
  */
void
StationHandover::check_for_handover() {

  int best_rssi = 0;
  EtherAddress best_bssid;

  // estimate rssi of current ap
  int old_rssi = 0;

  for (AccessPointMap::iterator i = _ap_map->begin(); i.live(); i++) {
    RSSIVector &rssi_vec = i.value();

    BRN_DEBUG(" %s with %d", i.key().s().c_str(), rssi_vec.size());

    if (rssi_vec.size() < _delta) // we need at least _delta entries for each AP
      continue;

    // calculate the rssi median value for each neighboring AP
    int rssi_avg = 0;
    for (int k = 0; k < rssi_vec.size(); k++) {
      rssi_avg += rssi_vec[k];
    }
    rssi_avg /= rssi_vec.size();

    BRN_DEBUG("* rssi_vec for %s has size %d and rssi_avg %d.", i.key().s().c_str(), rssi_vec.size(), rssi_avg);

    // save the rssi value for the current used AP
    if (i.key() == _winfo->_bssid) {
      old_rssi = rssi_avg;
    }

    if (rssi_avg > best_rssi) {
      best_rssi = rssi_avg;
      best_bssid = i.key();
    }
  }

  BRN_DEBUG("* best bssid is %s (%d), old is %s %d", best_bssid.s().c_str(), best_rssi,
    _winfo->_bssid.s().c_str(), old_rssi);

  if (_winfo->_bssid != best_bssid) {
    if (best_rssi > (old_rssi + _rssi_min_diff) ) {

      BRN_INFO("* need to reassoc: old %s (%d), new %s (%d)",
        _winfo->_bssid.s().c_str(), old_rssi, best_bssid.s().c_str(), best_rssi);

      if (!_winfo->_bssid) { //base station is currently not associated 
        BRN_DEBUG("* send_assoc_req");

        _winfo->_bssid = best_bssid;
        _station_assoc->send_assoc_req();

      } else { //base station is already associated
        BRN_DEBUG("* send_reassoc_req");

        _winfo->_bssid = best_bssid;
        _station_assoc->send_reassoc_req();
      }
    }
  }
}

/**
  * Helper function returns the BSSID of an incoming 802.11 beacon frame.
  */
EtherAddress
StationHandover::retrieve_bssid(Packet *p, String required_ssid) {
  uint8_t *ptr;
  struct click_wifi *w = (struct click_wifi *) p->data();
  ptr = (uint8_t *) (w+1);
  ptr += 12;

  uint8_t *end  = (uint8_t *) p->data() + p->length();

  uint8_t *ssid_l = NULL;
  uint8_t *rates_l = NULL;
  uint8_t *xrates_l = NULL;
  uint8_t *ds_l = NULL;
  while (ptr < end) {
    switch (*ptr) {
    case WIFI_ELEMID_SSID:
      ssid_l = ptr;
      break;
    case WIFI_ELEMID_RATES:
      rates_l = ptr;
      break;
    case WIFI_ELEMID_XRATES:
      xrates_l = ptr;
      break;
    case WIFI_ELEMID_FHPARMS:
      break;
    case WIFI_ELEMID_DSPARMS:
      ds_l = ptr;
      break;
    case WIFI_ELEMID_IBSSPARMS:
      break;
    case WIFI_ELEMID_TIM:
      break;
    case WIFI_ELEMID_ERP:
      break;
    case WIFI_ELEMID_VENDOR:
      break;
    case 133: /* ??? */
      break;
    case 150: /* ??? */
      break;
    }
    ptr += ptr[1] + 2;
  }

  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, min((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  if (ssid != required_ssid) {
    BRN_DEBUG("received non BRN beacon frame, ignore it. %s != %s", ssid.c_str(), required_ssid.c_str());
    return EtherAddress();
  } else {
    return EtherAddress(w->i_addr3);
  }
}

//-----------------------------------------------------------------------------
// Handler
//-----------------------------------------------------------------------------

enum {H_DEBUG, H_ACTIVE};

static String
read_debug_param(Element *e, void *thunk)
{
  StationHandover *ds = (StationHandover *)e;
  switch ((uintptr_t) thunk) {
  case H_DEBUG:
    return String(ds->_debug) + "\n";
  case H_ACTIVE:
    return String(ds->_active) + "\n";
  default:
    return String();
  }
}

static int 
write_debug_param(const String &in_s, Element *e, void *vparam,
		      ErrorHandler *errh)
{
  StationHandover *ds = (StationHandover *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    int debug;
    if (!cp_integer(s, &debug)) 
      return errh->error("debug parameter must be an integer value between 0 and 4");
    ds->_debug = debug;
    break;
  }
  case H_ACTIVE: {
    bool active;
    if (!cp_bool(s, &active)) 
      return errh->error("active parameter must be a boolean");
    ds->_active = active;
    break;
  }
  }
  return 0;
}

void
StationHandover::add_handlers()
{
  add_read_handler("debug", read_debug_param, (void*)H_DEBUG);
  add_write_handler("debug", write_debug_param, (void*)H_DEBUG);

  add_read_handler("active", read_debug_param, (void*)H_ACTIVE);
  add_write_handler("active", write_debug_param, (void*)H_ACTIVE);
}


#include <click/vector.cc>
#include <click/bighashmap.cc>
CLICK_ENDDECLS
EXPORT_ELEMENT(StationHandover)
ELEMENT_MT_SAFE(StationHandover)
