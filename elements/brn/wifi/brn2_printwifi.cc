
/*
 * OriginalVersion is:
 * printWifi.{cc,hh} -- print Wifi packets, for debugging.
 * John Bicket
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 *
 * Modified to get more information and to change to Printout
 */

#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/packet_anno.hh>
#include <clicknet/wifi.h>
#include <click/etheraddress.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include "elements/brn/wifi/brnwifi.hh"

#include "brn2_printwifi.hh"

CLICK_DECLS

BRN2PrintWifi::BRN2PrintWifi()
  : _label(""),
    _print_anno(false),
    _print_checksum(false),
    _timestamp(false),
    _print_ht(false),
    _print_ext_rx(false),
    _print_evm(false),
    _nowrap(false),
    _print_used_rate(false)
{
}

BRN2PrintWifi::~BRN2PrintWifi()
{
}

int
BRN2PrintWifi::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _timestamp = false;
  ret = cp_va_kparse(conf, this, errh,
      "LABEL", cpkP, cpString, &_label,
      "TIMESTAMP", cpkP, cpBool, &_timestamp,
      "PRINTHT", cpkP, cpBool, &_print_ht,
      "PRINTRXSTATUS", cpkP, cpBool, &_print_ext_rx,
      "PRINTEVM", cpkP, cpBool, &_print_evm,
      "PRINTUSEDRATE", cpkP, cpBool, &_print_used_rate,
      "NOWRAP", cpkP, cpBool, &_nowrap,
      cpEnd);
  return ret;
}

bool
valid_ssid(String *ssid)
{
  for ( int i = 0; i < ssid->length(); i++ )
    if ( (((uint8_t)(ssid->data()[i])) <= 32) || (((uint8_t)(ssid->data()[i])) >= 127) )
      return false;

  return true;
}

String
make_valid_ssid(String *ssid)
{
  StringAccum sa;
  for ( int i = 0; i < ssid->length(); i++ ) {
    if ( (((uint8_t)(ssid->data()[i])) <= 32) || (((uint8_t)(ssid->data()[i])) >= 127) ) {
      if ( ((uint8_t)(ssid->data()[i])) == 32)
        sa << '_';
      else
        sa << '*';
    } else {
      sa << ssid->data()[i];
    }
  }

  return sa.take_string();
}

String
BRN2PrintWifi::unparse_beacon(Packet *p) {
  uint8_t *ptr;
  struct click_wifi *w = (struct click_wifi *) p->data();
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  StringAccum sa;

  ptr = (uint8_t *) (w+1);

  //uint8_t *ts = ptr;
  ptr += 8;

  uint16_t beacon_int = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;

  uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
  ptr += 2;


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

  EtherAddress dst = EtherAddress(w->i_addr1);
  EtherAddress src = EtherAddress(w->i_addr2);
  EtherAddress bssid = EtherAddress(w->i_addr3);
  sa << dst << " " << src << " " << bssid << " ";

  String ssid = "";
  if (ssid_l && ssid_l[1]) {
    ssid = String((char *) ssid_l + 2, WIFI_MIN((int)ssid_l[1], WIFI_NWID_MAXSIZE));
  }

  //TODO: fix me, just to fix layout
  sa << " seq: 65535 ";

  if (ssid == "") {
    sa << "ssid: (none)";
  } else {
    if ( (ceh->magic == WIFI_EXTRA_MAGIC && ceh->flags & WIFI_EXTRA_RX_ERR)) //if crc-error then print empty ssid
      sa << "ssid: (empty)";
    else {
      if ( valid_ssid(&ssid) ) 
        sa << "ssid: " << ssid;
      else {
        String validssid = make_valid_ssid(&ssid);
        sa << "ssid(invalid): " << validssid;
      }
    }
  }

  int chan = (ds_l) ? ds_l[2] : 0;
  sa << " channel: " << chan;
  sa << " b_int: " << beacon_int << " ";

  Vector<int> basic_rates;
  Vector<int> rates;
  if (rates_l) {
    for (int x = 0; x < WIFI_MIN((int)rates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = rates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
        basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
        rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
    }
  }


  if (xrates_l) {
    for (int x = 0; x < WIFI_MIN((int)xrates_l[1], WIFI_RATE_SIZE); x++) {
      uint8_t rate = xrates_l[x + 2];

      if (rate & WIFI_RATE_BASIC) {
        basic_rates.push_back((int)(rate & WIFI_RATE_VAL));
      } else {
        rates.push_back((int)(rate & WIFI_RATE_VAL));
      }
    }
  }


  sa << "[ ";
  if (capability & WIFI_CAPINFO_ESS) {
    sa << "ESS ";
  }
  if (capability & WIFI_CAPINFO_IBSS) {
    sa << "IBSS ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLABLE) {
    sa << "CF_POLLABLE ";
  }
  if (capability & WIFI_CAPINFO_CF_POLLREQ) {
    sa << "CF_POLLREQ ";
  }
  if (capability & WIFI_CAPINFO_PRIVACY) {
    sa << "PRIVACY ";
  }
  sa << "] ";

  sa << "rates: ({";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << basic_rates[x];
    if (x != basic_rates.size()-1) {
      sa << " ";
    }
  }
  sa << "} ";
  for (int x = 0; x < rates.size(); x++) {
    sa << rates[x];
    if (x != rates.size()-1 ) {
      sa << " ";
    }
  }

  sa << ")";

  return sa.take_string();
}

String
BRN2PrintWifi::reason_string(int reason) {
  switch (reason) {
  case WIFI_REASON_UNSPECIFIED: return "unspecified";
  case WIFI_REASON_AUTH_EXPIRE:	return "auth_expire";
  case WIFI_REASON_AUTH_LEAVE:	return "auth_leave";
  case WIFI_REASON_ASSOC_EXPIRE: return "assoc_expire/inactive";
  case WIFI_REASON_ASSOC_TOOMANY: return "assoc_toomany";
  case WIFI_REASON_NOT_AUTHED:  return "not_authed";
  case WIFI_REASON_NOT_ASSOCED: return "not_assoced";
  case WIFI_REASON_ASSOC_LEAVE: return "assoc_leave";
  case WIFI_REASON_ASSOC_NOT_AUTHED: return "assoc_not_authed";
  default: return "unknown reason " + String(reason);
  }

}

String
BRN2PrintWifi::status_string(int status) {
  switch (status) {

  case WIFI_STATUS_SUCCESS: return "success";
  case WIFI_STATUS_UNSPECIFIED: return "unspecified";
  case WIFI_STATUS_CAPINFO: return "capinfo";
  case WIFI_STATUS_NOT_ASSOCED: return "not_assoced";
  case WIFI_STATUS_OTHER: return "other";
  case WIFI_STATUS_ALG: return "alg";
  case WIFI_STATUS_SEQUENCE: return "seq";
  case WIFI_STATUS_CHALLENGE: return "challenge";
  case WIFI_STATUS_TIMEOUT: return "timeout";
  case WIFI_STATUS_BASIC_RATES: return "basic_rates";
  case WIFI_STATUS_TOO_MANY_STATIONS: return "too_many_stations";
  case WIFI_STATUS_RATES: return "rates";
  case WIFI_STATUS_SHORTSLOT_REQUIRED: return "shortslot_required";
  default: return "unknown status " + String(status);    
  }
}
String 
BRN2PrintWifi::capability_string(int capability) {
  StringAccum sa;
  sa << "[";
  bool any = false;
  if (capability & WIFI_CAPINFO_ESS) {
    sa << "ESS";
    any = true;
  }
  if (capability & WIFI_CAPINFO_IBSS) {
    if (any) { sa << " ";}
    sa << "IBSS";
    any = true;
  }
  if (capability & WIFI_CAPINFO_CF_POLLABLE) {
    if (any) { sa << " ";}
    sa << "CF_POLLABLE";
    any = true;
  }
  if (capability & WIFI_CAPINFO_CF_POLLREQ) {
    if (any) { sa << " ";}
    sa << "CF_POLLREQ";
    any = true;
  }
  if (capability & WIFI_CAPINFO_PRIVACY) {
    if (any) { sa << " ";}
    sa << "PRIVACY";
  }
  sa << "]";
  return sa.take_string();
}

String
BRN2PrintWifi::get_ssid(u_int8_t *ptr) {
  if (ptr[0] != WIFI_ELEMID_SSID) {
    return "(invalid_ssid)";
  }
  return String((char *) ptr + 2, WIFI_MIN((int)ptr[1], WIFI_NWID_MAXSIZE));
}

Vector<int>
BRN2PrintWifi::get_rates(u_int8_t *ptr) {
  Vector<int> rates;
  for (int x = 0; x < WIFI_MIN((int)ptr[1], WIFI_RATES_MAXSIZE); x++) {
    uint8_t rate = ptr[x + 2];
    rates.push_back(rate);
  }
  return rates;
}

String
BRN2PrintWifi::rates_string(Vector<int> rates) {
  Vector<int> basic_rates;
  Vector<int> other_rates;
  StringAccum sa;
  for (int x = 0; x < rates.size(); x++) {
    if (rates[x] & WIFI_RATE_BASIC) {
      basic_rates.push_back(rates[x]);
    } else {
      other_rates.push_back(rates[x]);
    }
  }
  sa << "rates: ({";
  for (int x = 0; x < basic_rates.size(); x++) {
    sa << (basic_rates[x] & WIFI_RATE_VAL);
    if (x != basic_rates.size()-1) {
      sa << " ";
    }
  }
  sa << "} ";
  for (int x = 0; x < other_rates.size(); x++) {
    sa << other_rates[x];
    if (x != other_rates.size()-1) {
      sa << " ";
    }
  }
  sa << ")";
  return sa.take_string();
}

Packet *
BRN2PrintWifi::simple_action(Packet *p)
{
  struct click_wifi *wh = (struct click_wifi *) p->data();
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
  int type = wh->i_fc[0] & WIFI_FC0_TYPE_MASK;
  int subtype = wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK;
  //int duration = cpu_to_le16(wh->i_dur);
  EtherAddress src;
  EtherAddress dst;
  EtherAddress bssid;

  StringAccum sa;
  sa.reserve(512);

  if (_label[0] != 0) {
    sa << _label << ": ";
  }
  if (_timestamp)
    sa << p->timestamp_anno() << ": ";

  int len;
  len = sprintf(sa.reserve(9), "%4d | ", p->length());
  sa.adjust_length(len);

  uint8_t mcs_index, bandwidth, guard_interval;
  mcs_index = bandwidth = guard_interval = 0;

  if ( (ceh->flags & WIFI_EXTRA_TX) && _print_used_rate ) {
    uint8_t used_rate = ceh->rate;
    int tries_idx = ceh->max_tries;
    int idx = 0;

    if ( (ceh->max_tries1 != 0) && (ceh->retries >= tries_idx) )  {
      used_rate = ceh->rate1;
      tries_idx += ceh->max_tries1;
      idx = 1;

      if ( (ceh->max_tries2 != 0) && (ceh->retries >= tries_idx) )  {
        used_rate = ceh->rate2;
        tries_idx += ceh->max_tries2;
        idx = 2;

        if ( (ceh->max_tries3 != 0) && (ceh->retries >= tries_idx) )  {
          used_rate = ceh->rate3;
          idx = 3;
        }
      }
    }

    if ( BrnWifi::getMCS(ceh,idx) == 1 ) {
      BrnWifi::toMCS(&mcs_index, &bandwidth, &guard_interval, used_rate);
      int mcs_rate = BrnWifi::getMCSRate(mcs_index, bandwidth, guard_interval);

      int mcs_rate_b = mcs_rate/10;
      int mcs_rate_l = mcs_rate%10;

      sa << mcs_rate_b << "." << mcs_rate_l;

    } else {
      if (used_rate == 11) {
        sa << "5.5";
      } else {
        len = sprintf(sa.reserve(2), "%2d", used_rate/2);
        sa.adjust_length(len);
      }
    }
  } else {
    if ( ceh->flags & WIFI_EXTRA_MCS_RATE0 ) {
      BrnWifi::toMCS(&mcs_index, &bandwidth, &guard_interval, ceh->rate);
      int mcs_rate = BrnWifi::getMCSRate(mcs_index, bandwidth, guard_interval);

      int mcs_rate_b = mcs_rate/10;
      int mcs_rate_l = mcs_rate%10;

      sa << mcs_rate_b << "." << mcs_rate_l;

    } else {
      if (ceh->rate == 11) {
        sa << "5.5";
      } else {
        len = sprintf(sa.reserve(2), "%2d", ceh->rate/2);
        sa.adjust_length(len);
      }
    }
  }
  sa << "Mb ";

  if ( _print_ht ) {
    if ( ceh->flags & WIFI_EXTRA_MCS_RATE0 ) {
      sa << " 1 " << (uint32_t)mcs_index << " " << (uint32_t)bandwidth << " " << (uint32_t)guard_interval << " ";
    } else {
      sa << " 0 0 0 0 ";
    }
  }

  if ( _print_ext_rx ) {
    if ( ceh->flags & WIFI_EXTRA_EXT_RX_STATUS ) {
      struct brn_click_wifi_extra_rx_status *ext_status =
          (struct brn_click_wifi_extra_rx_status *)BRNPacketAnno::get_brn_wifi_extra_rx_status_anno(p);
      sa << ext_status->rssi_ctl[0] << " " << ext_status->rssi_ctl[1] << " " << ext_status->rssi_ctl[2] << " ";
      sa << ext_status->rssi_ext[0] << " " << ext_status->rssi_ext[1] << " " << ext_status->rssi_ext[2] << " ";
    } else {
      sa << " 0 0 0 0 0 0 ";
    }
  }

  if ( _print_evm ) {
    if ( ceh->flags & WIFI_EXTRA_EXT_RX_STATUS ) {
      struct brn_click_wifi_extra_rx_status *ext_status =
          (struct brn_click_wifi_extra_rx_status *)BRNPacketAnno::get_brn_wifi_extra_rx_status_anno(p);
      sa << ext_status->evm[0] << " " << ext_status->evm[1] << " " << ext_status->evm[2] << " ";
      sa << ext_status->evm[3] << " " << ext_status->evm[4] << " ";
    } else {
      sa << " 0 0 0 0 0 ";
    }
  }

  len = sprintf(sa.reserve(9), "+%02d/", ceh->rssi);
  sa.adjust_length(len);

  len = sprintf(sa.reserve(9), "%02d | ", ((signed char)ceh->silence));
  sa.adjust_length(len);



  switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
  case WIFI_FC1_DIR_NODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_TODS:
    bssid = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    dst = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_FROMDS:
    dst = EtherAddress(wh->i_addr1);
    bssid = EtherAddress(wh->i_addr2);
    src = EtherAddress(wh->i_addr3);
    break;
  case WIFI_FC1_DIR_DSTODS:
    dst = EtherAddress(wh->i_addr1);
    src = EtherAddress(wh->i_addr2);
    bssid = EtherAddress(wh->i_addr3);
    break;
  default:
    sa << "??? ";
  }

  uint8_t *ptr = (uint8_t *) p->data() + sizeof(click_wifi);
  switch (type) {
  case WIFI_FC0_TYPE_MGT:
    sa << "mgmt ";

    switch (subtype) {
    case WIFI_FC0_SUBTYPE_ASSOC_REQ: {
      
      uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      uint16_t l_int = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      String ssid = get_ssid(ptr);
      ptr += ptr[1] + 2;

      Vector<int> rates = get_rates(ptr);
      String rates_s = rates_string(rates);

      sa << "assoc_req ";

      sa << EtherAddress(wh->i_addr1);
      if (p->length() >= 16) {
        sa << " " << EtherAddress(wh->i_addr2);
      }

      if (p->length() > 22) {
        sa << " " << EtherAddress(wh->i_addr3);
      }
      sa << " ";

      if ( (ceh->magic == WIFI_EXTRA_MAGIC && ceh->flags & WIFI_EXTRA_RX_ERR)) { //if crc-error then print empty ssid
        sa << "seq: 65525 "; //TODO: fix
        sa << " ssid: (empty) ";
      } else {
        if ( valid_ssid(&ssid) )
          sa << " ssid: " << ssid;
        else
          sa << " ssid: (invalid_ssid) ";
      }

      sa << "listen_int: " << l_int << " ";
      sa << capability_string(capability);

      sa << rates_s;
      sa << " ";
      break;

    }
    case WIFI_FC0_SUBTYPE_ASSOC_RESP: {
      uint16_t capability = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      uint16_t status = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      uint16_t associd = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;
      sa << "assoc_resp ";

      sa << EtherAddress(wh->i_addr1);
      if (p->length() >= 16) {
        sa << " " << EtherAddress(wh->i_addr2);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      if (p->length() > 22) {
        sa << " " << EtherAddress(wh->i_addr3);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      sa << " ";
      sa << "seq: 65565 "; //TODO: fix
      sa << capability_string(capability);
      sa << " status " << (int) status << " " << status_string(status);
      sa << " associd " << associd << " ";
      break;
    }
    case WIFI_FC0_SUBTYPE_REASSOC_REQ:    sa << "reassoc_req "; break;
    case WIFI_FC0_SUBTYPE_REASSOC_RESP:   sa << "reassoc_resp "; break;
    case WIFI_FC0_SUBTYPE_PROBE_REQ:      {
      sa << "probe_req "; 

      sa << EtherAddress(wh->i_addr1);
      if (p->length() >= 16) {
        sa << " " << EtherAddress(wh->i_addr2);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      if (p->length() > 22) {
        sa << " " << EtherAddress(wh->i_addr3);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      sa << " ";

      String ssid = get_ssid(ptr);

      ptr += ptr[1] + 2;

      Vector<int> rates = get_rates(ptr);
      String rates_s = rates_string(rates);
      //TODO: enable again
   /*   if ( (ceh->magic == WIFI_EXTRA_MAGIC && ceh->flags & WIFI_EXTRA_RX_ERR)) //if crc-error then print empty ssid
        sa << "ssid: (empty)";
      else {
        if ( valid_ssid(&ssid) )
          sa << "ssid: " << ssid;
        else
          sa << "ssid: (invalid_ssid)";
      }

      sa << " " << rates_s << " ";*/
      break;

    }
    case WIFI_FC0_SUBTYPE_PROBE_RESP:
      sa << "probe_resp ";
      sa << unparse_beacon(p);
      sa << "seq: 65565 "; //TODO: just to fit layout
      goto done;
    case WIFI_FC0_SUBTYPE_BEACON:
      sa << "beacon ";
      sa << unparse_beacon(p);
      sa << "seq: 65565 "; //TODO: just to fit layout
      goto done;
    case WIFI_FC0_SUBTYPE_ATIM:           sa << "atim "; break;
    case WIFI_FC0_SUBTYPE_DISASSOC:       {
      //uint16_t reason = le16_to_cpu(*(uint16_t *) ptr);
      sa << "disassoc " ;

      sa << EtherAddress(wh->i_addr1);
      if (p->length() >= 16) {
        sa << " " << EtherAddress(wh->i_addr2);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      if (p->length() > 22) {
        sa << " " << EtherAddress(wh->i_addr3);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      sa << " ";

      //TODO: enable again
      //sa << reason_string(reason) << " ";
      break;
    }
    case WIFI_FC0_SUBTYPE_AUTH: {
      sa << "auth ";

      sa << EtherAddress(wh->i_addr1);
      if (p->length() >= 16) {
        sa << " " << EtherAddress(wh->i_addr2);
      } else {
        sa << " 00-00-00-00-00-00";
      }
      if (p->length() > 22) {
        sa << " " << EtherAddress(wh->i_addr3);
      } else {
        sa << " 00-00-00-00-00-00";
      }

      sa << " ";

      //uint16_t algo = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      //uint16_t seq = le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;

      //uint16_t status =le16_to_cpu(*(uint16_t *) ptr);
      ptr += 2;
      //TODO: enable again
/*      sa << "alg " << (int)  algo;
      sa << " auth_seq: " << (int) seq;
      sa << " status " << status_string(status) << " ";*/
      break;

    }
    case WIFI_FC0_SUBTYPE_DEAUTH:         sa << "deauth "; break;
    default:
      sa << "unknown-subtype-" << (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) << " ";
      break;
    }
    break;
  case WIFI_FC0_TYPE_CTL:
    sa << "cntl ";
    switch (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) {
    case WIFI_FC0_SUBTYPE_PS_POLL:    sa << "psp  "; break;
    case WIFI_FC0_SUBTYPE_RTS:        sa << "rts  "; break;
    case WIFI_FC0_SUBTYPE_CTS:	      sa << "cts  "; break;
    case WIFI_FC0_SUBTYPE_ACK:	      sa << "ack  " /*<< duration << " "*/; break;
    case WIFI_FC0_SUBTYPE_CF_END:     sa << "cfe  "; break;
    case WIFI_FC0_SUBTYPE_CF_END_ACK: sa << "cfea "; break;
    default:
    sa << "unknown-subtype-" << (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) << " ";
    }
    break;
  case WIFI_FC0_TYPE_DATA:
    sa << "data ";
    switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
    case WIFI_FC1_DIR_NODS:   sa << "nods "; break;
    case WIFI_FC1_DIR_TODS:   sa << "tods "; break;
    case WIFI_FC1_DIR_FROMDS: sa << "frds "; break;
    case WIFI_FC1_DIR_DSTODS: sa << "dsds "; break;
    }
    break;
  default:
    sa << "unknown-type-" << (int) (wh->i_fc[0] & WIFI_FC0_TYPE_MASK) << " unknown-subtype-" << (int) (wh->i_fc[0] & WIFI_FC0_SUBTYPE_MASK) << " ";
  }

  if ((type == WIFI_FC0_TYPE_MGT) &&
      (subtype == WIFI_FC0_SUBTYPE_BEACON || subtype == WIFI_FC0_SUBTYPE_PROBE_RESP)) {

    if ( _nowrap )
      BrnLogger::chatter("%s", sa.c_str());
    else
      click_chatter("%s\n", sa.c_str());

    return p;
  }


  if ( !((type == WIFI_FC0_TYPE_MGT) &&
             ((subtype == WIFI_FC0_SUBTYPE_PROBE_REQ) ||
              (subtype == WIFI_FC0_SUBTYPE_ASSOC_RESP) ||
              (subtype == WIFI_FC0_SUBTYPE_ASSOC_REQ) ||
              (subtype == WIFI_FC0_SUBTYPE_AUTH) ||
              (subtype == WIFI_FC0_SUBTYPE_DISASSOC)))) {
    sa << EtherAddress(wh->i_addr1);
    if (p->length() >= 16) {
      sa << " " << EtherAddress(wh->i_addr2);
    } else {
      sa << " 00-00-00-00-00-00";
    }
    if (p->length() > 22) {
      sa << " " << EtherAddress(wh->i_addr3);
    } else {
      sa << " 00-00-00-00-00-00";
    }
    sa << " ";
  }

  if (p->length() >= sizeof(click_wifi)) {
    uint16_t seq = le16_to_cpu(wh->i_seq) >> WIFI_SEQ_SEQ_SHIFT;
    uint8_t frag = le16_to_cpu(wh->i_seq) & WIFI_SEQ_FRAG_MASK;
    sa << "seq: " << (int) seq;
    if (frag || wh->i_fc[1] & WIFI_FC1_MORE_FRAG) {
      sa << " frag: " << (int) frag;
    }
    sa << " ";
  } else {
    sa << "seq: 65565 ";
  }

  sa << "[";
  if (ceh->flags & WIFI_EXTRA_TX) {
    sa << " tx";
  }
  if (ceh->flags & WIFI_EXTRA_TX_FAIL) {
    if (ceh->flags & WIFI_EXTRA_TX_ABORT) {
      sa << " abort";
    } else {
      sa << " fail";
    }
  }
  if (ceh->flags & WIFI_EXTRA_TX_USED_ALT_RATE) {
    sa << " alt_rate";
  }
  if (ceh->flags & WIFI_EXTRA_RX_ERR) {
    sa << " err";
  }
  if (ceh->flags & WIFI_EXTRA_RX_MORE) {
    sa << " more";
  }
  if (wh->i_fc[1] & WIFI_FC1_RETRY) {
    sa << " retry";
  }
  if (wh->i_fc[1] & WIFI_FC1_WEP) {
    sa << " wep";
  }
  sa << " ] ";

  if (ceh->flags & WIFI_EXTRA_TX) {
    sa << " retries " << (int) ceh->retries;
  }
  if (ceh->flags & WIFI_EXTRA_EXT_RETRY_INFO) {
    sa << " rts/data " << (int)(ceh->virt_col >> 4) << "/" << (int)(ceh->virt_col & 15) << " " << (int)ceh->virt_col;
  }

 done:
  if ( _nowrap )
    BrnLogger::chatter("%s", sa.c_str());
  else
    click_chatter("%s\n", sa.c_str());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(BRN2PrintWifi)
