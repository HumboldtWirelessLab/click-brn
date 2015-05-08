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

#include "elements/brn/standard/brnlogger/brnlogger.hh"

#include "ath2print.hh"
#include "ath2_desc.h"

CLICK_DECLS

Ath2Print::Ath2Print() :
  _includeath(true),
  _timestamp(false),
  _txprint(true),
  _rxprint(true),
  _parser(false),
  _nowrap(false)

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
  _includeath = false;

  ret = cp_va_kparse(conf, this, errh,
                     "LABEL", cpkN, cpString, &_label,
                     "INCLUDEATH", cpkN, cpBool, &_includeath,
                     "TIMESTAMP", cpkN, cpBool, &_timestamp,
                     "PRINTTX", cpkN, cpBool, &_txprint,
                     "PRINTRX", cpkN, cpBool, &_rxprint,
                     "NOWRAP", cpkN, cpBool, &_nowrap,
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

  StringAccum sa_time_stamp;
  StringAccum sa_ath1;
  StringAccum sa_ath2;

  bool tx = false; //mostly packets are received

  /*Check the length*/
  if ( ( _includeath && ( p->length() < ( ATHDESC_HEADER_SIZE + sizeof(struct ath2_header) ) ) ) ||
         ( ( ! _includeath )  && ( p->length() < ( sizeof(struct ath2_header) ) ) ) )
  {
    /*packet too short: no ATH/ath2*/
    if ( noutputs() > 1 )
      output(1).push(p);
    else
      p->kill();

    return NULL;
  }

  if (_timestamp)
    sa_time_stamp << p->timestamp_anno() << " ";
  else
    sa_time_stamp << "";

  /*The ath2 header follows the ath header, so first check and print tha header if requested*/
  if ( _includeath )
  {
    WritablePacket *q = p->uniqueify();
    if (q)
    {
      struct ar5212_desc *desc = (struct ar5212_desc *) (q->data() + 8);

      /*framelen of 0 indicates RX-Frame. If len is != 0 then it's a TXFeedback*/

      sa_ath1 << "ATH: ";

      if (desc->frame_len == 0)
      {
        tx = false;
        if ( ! _rxprint ) {
          p->kill();
          return NULL;
        }

        struct ar5212_rx_status *rx_desc = (struct ar5212_rx_status *) (q->data() + 16);

        if (!rx_desc->rx_ok)  sa_ath1 << "(RX) Status: 1 (Err) ";
        else                  sa_ath1 << "(RX) Status: 0 (OK) ";

        sa_ath1 << "Rate: " << ratecode_to_dot11(rx_desc->rx_rate);
        sa_ath1 << " RSSI: " << rx_desc->rx_rssi;
        sa_ath1 << " LEN: " << rx_desc->data_len;
        sa_ath1 << " More: " << rx_desc->more;
        sa_ath1 << " DCErr: " << rx_desc->decomperr;
        sa_ath1 << " Ant: " << rx_desc->rx_ant;
        sa_ath1 << " Done: " << rx_desc->done;
        sa_ath1 << " CRCErr: " << rx_desc->crcerr;
        sa_ath1 << " DecryptCRC: " << rx_desc->decryptcrc;
        /* 4 + 2 * 9 = 22 cols */
      }
      else
      {
        tx = true;

        if ( ! _txprint ) {
          p->kill();
          return NULL;
        }

        sa_ath1 << "(TX) : ";
        sa_ath1 << "Power: " << desc->xmit_power;
        sa_ath1 << " AckRSSI: " << desc->ack_sig_strength;
        sa_ath1 << " Rate: " << ratecode_to_dot11(desc->xmit_rate0) << " (" << desc->xmit_tries0 << ")";
        sa_ath1 << " Rate1: " << ratecode_to_dot11(desc->xmit_rate1) << " (" << desc->xmit_tries1 << ")";
        sa_ath1 << " Rate2: " << ratecode_to_dot11(desc->xmit_rate2) << " (" << desc->xmit_tries2 << ")";
        sa_ath1 << " Rate3: " << ratecode_to_dot11(desc->xmit_rate3) << " (" << desc->xmit_tries3 << ")";
        sa_ath1 << " Failcount: " << desc->data_fail_count;
        sa_ath1 << " EXRetries: " << desc->excessive_retries;
        /* 2 * 5 + 4 * 3 = 22 cols */
      }
    }

    ath2_h = (struct ath2_header*)&(p->data()[ATHDESC_HEADER_SIZE]);

  } else { //if include_ath
    sa_ath1 << "";

    if (ceha->magic == WIFI_EXTRA_MAGIC && ceha->flags & WIFI_EXTRA_TX)
      tx = true;
    else
      tx = false;

    ath2_h = (struct ath2_header*)p->data();

  }

  if ( ntohs(ath2_h->ath2_version) == ATHDESC2_VERSION )
  {
    if (tx)
    {
      sa_ath2 << "(TX) Seq: ";
      sa_ath2 << (int)/*ntohs*/(ath2_h->anno.tx.ts_seqnum);
      sa_ath2 << " TS: ";sa_ath2 << (int)ntohs(ath2_h->anno.tx.ts_tstamp);
      sa_ath2 << " Status: ";
      sa_ath2 << (int)ath2_h->anno.tx.ts_status;sa_ath2 << " (" << tx_errcode_to_string(ath2_h->anno.tx.ts_status) << ")";
      sa_ath2 << " Rate: ";sa_ath2 << (int)ratecode_to_dot11(ath2_h->anno.tx.ts_rate);
      sa_ath2 << " RSSI: ";sa_ath2 << (int)ath2_h->anno.tx.ts_rssi;
      sa_ath2 << " Ant: ";sa_ath2 << (int)ath2_h->anno.tx.ts_antenna;
      sa_ath2 << " Noise: ";sa_ath2 << (int)ath2_h->anno.tx.ts_noise;
      sa_ath2 << " Hosttime: ";sa_ath2 << (u_int64_t)ath2_h->anno.tx.ts_hosttime;
      sa_ath2 << " Mactime: ";sa_ath2 << (u_int64_t)ath2_h->anno.tx.ts_mactime;
      sa_ath2 << " Channel: ";sa_ath2 << (u_int64_t)ath2_h->anno.tx.ts_channel;
      sa_ath2 << " ChannelUtility: "; sa_ath2 << (uint32_t)ath2_h->anno.tx.ts_channel_utility;

      sa_ath2 << " DriverFlags: "; sa_ath2 << ntohl((uint32_t)ath2_h->flags);
      sa_ath2 << " Flags: "; sa_ath2 << (uint32_t)ath2_h->anno.tx.ts_flags;

      sa_ath2 << " SRetry: ";sa_ath2 << (int)ath2_h->anno.tx.ts_shortretry;
      sa_ath2 << " LRetry: ";sa_ath2 << (int)ath2_h->anno.tx.ts_longretry;
      sa_ath2 << " VCC: ";sa_ath2 << (int)ath2_h->anno.tx.ts_virtcol;
      sa_ath2 << " FinTSI: ";sa_ath2 << (int)ath2_h->anno.tx.ts_finaltsi;
      /* 2 + 1 + 2 + 1 + 2 + 2 * 11 = 30 cols */
    }
    else
    {
      sa_ath2 << "(RX) Len: ";
      sa_ath2 << (int)/*ntohs*/(ath2_h->anno.rx.rs_datalen);
      sa_ath2 << " TS: ";sa_ath2 << (unsigned int)/*ntohl*/(ath2_h->anno.rx.rs_tstamp);
      sa_ath2 << " Status: ";
      sa_ath2 << (int)ath2_h->anno.rx.rs_status; sa_ath2 << " (" << rx_errcode_to_string(ath2_h->anno.rx.rs_status) << ")";
      sa_ath2 << " Rate: ";sa_ath2 << (int)ratecode_to_dot11(ath2_h->anno.rx.rs_rate);
      sa_ath2 << " RSSI: ";sa_ath2 << (int)ath2_h->anno.rx.rs_rssi;
      sa_ath2 << " Ant: ";sa_ath2 << (unsigned int)/*ntohl*/(ath2_h->anno.rx.rs_antenna);
      sa_ath2 << " Noise: ";sa_ath2 << (int)ath2_h->anno.rx.rs_noise;
      sa_ath2 << " Hosttime: ";sa_ath2 << (u_int64_t)ath2_h->anno.rx.rs_hosttime;
      sa_ath2 << " Mactime: ";sa_ath2 << (u_int64_t)ath2_h->anno.rx.rs_mactime;
      sa_ath2 << " Channel: ";sa_ath2 << (u_int64_t)ath2_h->anno.rx.rs_channel;
      sa_ath2 << " ChannelUtility: "; sa_ath2 << (uint32_t)ath2_h->anno.rx.rs_channel_utility;

      sa_ath2 << " DriverFlags: "; sa_ath2 << ntohl((uint32_t)ath2_h->flags);
      sa_ath2 << " Flags: "; sa_ath2 << (uint32_t)ath2_h->anno.rx.rs_flags;

      sa_ath2 << " Phyerr: ";sa_ath2 << (int)ath2_h->anno.rx.rs_phyerr;
      sa_ath2 << " PhyerrStr: ";
      if ( ath2_h->anno.rx.rs_status == 2 )
        sa_ath2 << " (" << phy_errcode_to_string(ath2_h->anno.rx.rs_phyerr) << ")";
      else
        sa_ath2 << " (none)";
      sa_ath2 << " More: ";sa_ath2 << (int)ath2_h->anno.rx.rs_more;
      sa_ath2 << " Keyix: ";sa_ath2 << (int)ath2_h->anno.rx.rs_keyix;
      /* 2 + 1 + 2 + 2 + 2 + 1 + 10 * 2 = 30 cols */
    }
  }

  if ( ! _nowrap ) {
    if ( _label[0] != 0 )
      click_chatter("%s: %s %s %s", _label.c_str(), sa_time_stamp.c_str(), sa_ath1.c_str(), sa_ath2.c_str());
    else
      click_chatter("%s %s %s", sa_time_stamp.c_str(), sa_ath1.c_str(), sa_ath2.c_str());
  } else {
    if ( _label[0] != 0 )
      BrnLogger::chatter("%s: %s %s %s ", _label.c_str(), sa_time_stamp.c_str(), sa_ath1.c_str(), sa_ath2.c_str());
    else
      BrnLogger::chatter("%s %s %s ", sa_time_stamp.c_str(), sa_ath1.c_str(), sa_ath2.c_str());
  }

  return p;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel linux)
EXPORT_ELEMENT(Ath2Print)
ELEMENT_MT_SAFE(Ath2Print)
