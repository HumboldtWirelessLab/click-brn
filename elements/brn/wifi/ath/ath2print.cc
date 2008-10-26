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
 * ath2print.{cc,hh} -- element removes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include "ath2print.hh"
#include "ah_desc.h"
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/athdesc.h>

CLICK_DECLS

Ath2Print::Ath2Print()
{
}

Ath2Print::~Ath2Print()
{
}
int
Ath2Print::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _label = "";
  _printath = 0;

  ret = cp_va_kparse(conf, this, errh,
                     "PRINTATH", cpkP, cpInteger, &_printath,
                     "LABEL", cpkP, cpString, &_label,
                     cpEnd);
  return ret;
}


inline String
rx_errcode_to_string(int errcode)
{
  switch (errcode) {
    case 0: return "OK";
    case 1: return "CRC";
    case 2: return "PHY";
    case 4: return "FIFO";
    case 8: return "DECRYPT";
    case 16: return "MIC";
  }
  return "Unknown";
}

inline String
tx_errcode_to_string(int errcode)
{
  switch (errcode) {
    case 0: return "OK";
    case 1: return "XRETRY";
    case 2: return "FILT";
    case 4: return "FIFO";
    case 8: return "TXOP";
    case 16: return "CFG";
    case 32: return "TXB";
    case 64: return "TXD";
  }
  return "Unknown";
}

inline String
phy_errcode_to_string(int errcode)
{
  switch (errcode) {
    case 0: return "HAL_PHYERR_UNDERRUN";  /* Transmit underrun */
    case 1: return "HAL_PHYERR_TIMING";    /* Timing error */
    case 2: return "HAL_PHYERR_PARITY";    /* Illegal parity */
    case 3: return "HAL_PHYERR_RATE";      /* Illegal rate */
    case 4: return "HAL_PHYERR_LENGTH";    /* Illegal length */
    case 5: return "HAL_PHYERR_RADAR";     /* Radar detect */
    case 6: return "HAL_PHYERR_SERVICE";   /* Illegal service */
    case 7: return "HAL_PHYERR_TOR";       /* Transmit override receive */
    case 17: return "HAL_PHYERR_OFDM_TIMING";         /* */
    case 18: return "HAL_PHYERR_OFDM_SIGNAL_PARITY";  /* */
    case 19: return "HAL_PHYERR_OFDM_RATE_ILLEGAL";   /* */
    case 20: return "HAL_PHYERR_OFDM_LENGTH_ILLEGAL"; /* */
    case 21: return "HAL_PHYERR_OFDM_POWER_DROP";     /* */
    case 22: return "HAL_PHYERR_OFDM_SERVICE";    /* */
    case 23: return "HAL_PHYERR_OFDM_RESTART";    /* */
    case 25: return "HAL_PHYERR_CCK_TIMING";      /* */
    case 26: return "HAL_PHYERR_CCK_HEADER_CRC"; /* */
    case 27: return "HAL_PHYERR_CCK_RATE_ILLEGAL";/* */
    case 30: return "HAL_PHYERR_CCK_SERVICE";     /* */
    case 31: return "HAL_PHYERR_CCK_RESTART";     /* */
  }
  return "Unknown";
}


Packet *
Ath2Print::simple_action(Packet *p)
{
  struct click_wifi_extra *ceha = WIFI_EXTRA_ANNO(p);
  struct click_wifi_extra *cehp = (struct click_wifi_extra *) p->data();
  struct ath_tx_status *tx_status = NULL;
  struct ath_rx_status *rx_status = NULL;
  StringAccum sa;

  if ((ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_TX) ||
      (cehp->magic == WIFI_EXTRA_MAGIC && cehp->flags & WIFI_EXTRA_TX))
  {
    tx_status = (struct ath_tx_status *)p->data();
    sa << "(TX) Seq: ";
    sa << (int)/*ntohs*/(tx_status->ts_seqnum);
    sa << " TS: ";sa << (int)ntohs(tx_status->ts_tstamp);
    sa << " Status: ";sa << (int)tx_status->ts_status;sa << " (" << tx_errcode_to_string(tx_status->ts_status) << ")";
    sa << " Rate: ";sa << (int)ratecode_to_dot11(tx_status->ts_rate);
    sa << " RSSI: ";sa << (int)tx_status->ts_rssi;
    sa << " SRetry: ";sa << (int)tx_status->ts_shortretry;
    sa << " LRetry: ";sa << (int)tx_status->ts_longretry;
    sa << " VCC: ";sa << (int)tx_status->ts_virtcol;
    sa << " ant: ";sa << (int)tx_status->ts_antenna;
    sa << " FinTSI: ";sa << (int)tx_status->ts_finaltsi;
  }
  else
  {
    rx_status = (struct ath_rx_status *)p->data();
    sa << "(RX) Len: ";
    sa << (int)/*ntohs*/(rx_status->rs_datalen);
    sa << " Status: ";sa << (int)rx_status->rs_status;sa << " (" << rx_errcode_to_string(rx_status->rs_status) << ")";
    sa << " Phyerr: ";sa << (int)rx_status->rs_phyerr;
    if ( rx_status->rs_status == 2 )
      sa << " (" << phy_errcode_to_string(rx_status->rs_phyerr) << ")";
    else
      sa << " (none)";
    sa << " RSSI: ";sa << (int)rx_status->rs_rssi;
    sa << " Keyix: ";sa << (int)rx_status->rs_keyix;
    sa << " Rate: ";sa << (int)ratecode_to_dot11(rx_status->rs_rate);
    sa << " More: ";sa << (int)rx_status->rs_more;
    sa << " TS: ";sa << (unsigned int)/*ntohl*/(rx_status->rs_tstamp);
    sa << " Ant: ";sa << (unsigned int)/*ntohl*/(rx_status->rs_antenna);
  }

  if ( _label[0] != 0 )
    click_chatter("%s: %s\n", _label.c_str(),sa.c_str());
  else
    click_chatter("%s\n", sa.c_str());

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Print)
ELEMENT_MT_SAFE(Ath2Print)
