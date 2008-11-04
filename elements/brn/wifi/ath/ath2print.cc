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
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/athdesc.h>

#include "ath2print.hh"
#include "ath2_desc.h"

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
  _complath = 0;

  ret = cp_va_kparse(conf, this, errh,
                     "LABEL", cpkN, cpString, &_label,
                     "COMPLATH", cpkN, cpInteger, &_complath,
                     "TIMESTAMP", cpkN, cpBool, &_timestamp,
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
  struct ath2_header *ath2_h = NULL;

  StringAccum sa;
  StringAccum sa_ath1;
  bool tx;

  if ( _complath == 1 )
  {
    WritablePacket *q = p->uniqueify();
    if (q)
    {
      if (_timestamp)
        sa_ath1 << p->timestamp_anno() << ": ";

      sa_ath1 << "ATHDESC: ";
      struct ar5212_desc *desc = (struct ar5212_desc *) (q->data() + 8);

      if (desc->frame_len == 0)
      {
        tx = false;

        struct ar5212_rx_status *rx_desc = (struct ar5212_rx_status *) (q->data() + 16);

        if (!rx_desc->rx_ok)  sa_ath1 << "(RX) Status: (Err) ";
        else                  sa_ath1 << "(RX) Status: (OK) ";

        sa_ath1 << "Rate: " << ratecode_to_dot11(rx_desc->rx_rate);
        sa_ath1 << " RSSI: " << rx_desc->rx_rssi;
        sa_ath1 << " LEN: " << rx_desc->data_len;
        sa_ath1 << " More: " << rx_desc->more;
        sa_ath1 << " DCErr: " << rx_desc->decomperr;
        sa_ath1 << " Ant: " << rx_desc->rx_ant;
        sa_ath1 << " Done: " << rx_desc->done;
        sa_ath1 << " CRCErr: " << rx_desc->crcerr;
        sa_ath1 << " DecryptCRC: " << rx_desc->decryptcrc;
      }
      else
      {
        tx = true;

        sa_ath1 << "(TX) ";
        sa_ath1 << "Power: " << desc->xmit_power;
        sa_ath1 << " ACKRSSI: " << desc->ack_sig_strength;
        sa_ath1 << " Rate: " << ratecode_to_dot11(desc->xmit_rate0);
        sa_ath1 << " FAILCOUNT: " << desc->data_fail_count;
        sa_ath1 << " EXRetries: " << desc->excessive_retries;
      }
    }

    if ( _label[0] != 0 )
      click_chatter("%s: %s\n", _label.c_str(),sa_ath1.c_str());
    else
      click_chatter("%s\n", sa_ath1.c_str());
  }
  else
  {
    if (ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_TX)
      tx = true;
    else
      tx = false;
  }

  if ( _complath == 1 )
    ath2_h = (struct ath2_header*)&(p->data()[ATHDESC_HEADER_SIZE]);
  else
    ath2_h = (struct ath2_header*)p->data();

  if ( ath2_h->ath2_version == ATHDESC2_VERSION )
  {
    if (_timestamp)
      sa << p->timestamp_anno() << ": ";

    if (tx)
    {
      sa << "(TX) Seq: ";
      sa << (int)/*ntohs*/(ath2_h->anno.tx.ts_seqnum);
      sa << " TS: ";sa << (int)ntohs(ath2_h->anno.tx.ts_tstamp);
      sa << " Status: ";
      sa << (int)ath2_h->anno.tx.ts_status;sa << " (" << tx_errcode_to_string(ath2_h->anno.tx.ts_status) << ")";
      sa << " Rate: ";sa << (int)ratecode_to_dot11(ath2_h->anno.tx.ts_rate);
      sa << " RSSI: ";sa << (int)ath2_h->anno.tx.ts_rssi;
      sa << " SRetry: ";sa << (int)ath2_h->anno.tx.ts_shortretry;
      sa << " LRetry: ";sa << (int)ath2_h->anno.tx.ts_longretry;
      sa << " VCC: ";sa << (int)ath2_h->anno.tx.ts_virtcol;
      sa << " ant: ";sa << (int)ath2_h->anno.tx.ts_antenna;
      sa << " FinTSI: ";sa << (int)ath2_h->anno.tx.ts_finaltsi;
      sa << " Noise: ";sa << (int)ath2_h->anno.tx.ts_noise;
      sa << " Hosttime: ";sa << (u_int64_t)ath2_h->anno.tx.ts_hosttime;
      sa << " Mactime: ";sa << (u_int64_t)ath2_h->anno.tx.ts_mactime;
    }
    else
    {
      sa << "(RX) Len: ";
      sa << (int)/*ntohs*/(ath2_h->anno.rx.rs_datalen);
      sa << " Status: ";
      sa << (int)ath2_h->anno.rx.rs_status;sa << " (" << rx_errcode_to_string(ath2_h->anno.rx.rs_status) << ")";
      sa << " Phyerr: ";sa << (int)ath2_h->anno.rx.rs_phyerr;
      if ( ath2_h->anno.rx.rs_status == 2 )
        sa << " (" << phy_errcode_to_string(ath2_h->anno.rx.rs_phyerr) << ")";
      else
        sa << " (none)";
      sa << " RSSI: ";sa << (int)ath2_h->anno.rx.rs_rssi;
      sa << " Rate: ";sa << (int)ratecode_to_dot11(ath2_h->anno.rx.rs_rate);
      sa << " More: ";sa << (int)ath2_h->anno.rx.rs_more;
      sa << " Keyix: ";sa << (int)ath2_h->anno.rx.rs_keyix;
      sa << " TS: ";sa << (unsigned int)/*ntohl*/(ath2_h->anno.rx.rs_tstamp);
      sa << " Ant: ";sa << (unsigned int)/*ntohl*/(ath2_h->anno.rx.rs_antenna);
      sa << " Noise: ";sa << (int)ath2_h->anno.rx.rs_noise;
      sa << " Hosttime: ";sa << (u_int64_t)ath2_h->anno.rx.rs_hosttime;
      sa << " Mactime: ";sa << (u_int64_t)ath2_h->anno.rx.rs_mactime;
    }

    if ( _label[0] != 0 )
      click_chatter("%s: %s\n", _label.c_str(),sa.c_str());
    else
      click_chatter("%s\n", sa.c_str());
  }

  return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Print)
ELEMENT_MT_SAFE(Ath2Print)
