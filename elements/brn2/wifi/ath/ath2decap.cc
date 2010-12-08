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
 * stripbrnheader.{cc,hh} -- element removes (leading) BRN header at offset position 0.
 */

#include <click/config.h>
#include <click/straccum.hh>
#include <clicknet/wifi.h>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include <elements/wifi/athdesc.h>
#include <elements/brn2/wifi/brnwifi.h>
#include <elements/brn2/brnprotocol/brnpacketanno.hh>
#include "elements/brn2/standard/brnlogger/brnlogger.hh"

#include "ath2decap.hh"
#include "ath2_desc.h"

CLICK_DECLS

Ath2Decap::Ath2Decap():
    _cst(NULL)
{
  BRNElement::init();
}

Ath2Decap::~Ath2Decap()
{
}

int
Ath2Decap::configure(Vector<String> &conf, ErrorHandler* errh)
{
  int ret;
  _athdecap = false;

  ret = cp_va_kparse(conf, this, errh,
                     "ATHDECAP", cpkP, cpBool, &_athdecap,
                     "CHANNELSTATS", cpkP, cpElement, &_cst,
                     cpEnd);
  return ret;
}

Packet *
Ath2Decap::simple_action(Packet *p)
{
  WritablePacket *q;
  click_wifi_extra *eh;
  struct ath2_header *ath2_h = NULL;

  /** Filter Operations */
  if ( ( _athdecap && ( p->length() == ( ATHDESC_HEADER_SIZE + sizeof(struct ath2_header) ) ) ) ||
         ( ( ! _athdecap ) && ( p->length() == ( sizeof(struct ath2_header) ) ) ) )
  {
    if ( noutputs() > 2 )
      output(2).push(p);
    else
      p->kill();

    return NULL;
  }

  /** Filter too small packets */
  if ( ( _athdecap && ( p->length() < ( ATHDESC_HEADER_SIZE + sizeof(struct ath2_header) ) ) ) ||
       ( ( ! _athdecap ) && ( p->length() < ( sizeof(struct ath2_header) ) ) ) )
  {
    if ( noutputs() > 1 )
      output(1).push(p);
    else
      p->kill();

    return NULL;
  }

  q = p->uniqueify();
  if ( !q ) return p;

  if ( _athdecap )
  {
    struct ar5212_desc *desc = (struct ar5212_desc *) (q->data() + 8);
    eh = WIFI_EXTRA_ANNO(q);
    memset(eh, 0, sizeof(click_wifi_extra));
    eh->magic = WIFI_EXTRA_MAGIC;

    if (desc->frame_len == 0) {
      struct ar5212_rx_status *rx_desc = (struct ar5212_rx_status *) (q->data() + 16);
      /* rx */
      eh->rate = ratecode_to_dot11(rx_desc->rx_rate);

      if ( eh->rate == 0 ) {
        BRN_INFO("Unknown ratecode: %d",rx_desc->rx_rate);
        eh->flags |= WIFI_EXTRA_RX_ERR;
        eh->flags |= WIFI_EXTRA_RX_ZERORATE_ERR;
      }

      eh->rssi = rx_desc->rx_rssi;
      if (!rx_desc->rx_ok) {
        eh->flags |= WIFI_EXTRA_RX_ERR;
      }
    } else {
      eh->flags |= WIFI_EXTRA_TX;
      /* tx */
      eh->power = desc->xmit_power;
      eh->rssi = desc->ack_sig_strength;
      eh->retries = desc->data_fail_count;

      if (desc->excessive_retries)
        eh->flags |= WIFI_EXTRA_TX_FAIL;

      eh->rate = ratecode_to_dot11(desc->xmit_rate0);
      eh->rate1 = ratecode_to_dot11(desc->xmit_rate1);
      eh->rate2 = ratecode_to_dot11(desc->xmit_rate2);
      eh->rate3 = ratecode_to_dot11(desc->xmit_rate3);

      eh->max_tries = desc->xmit_tries0;
      eh->max_tries1 = desc->xmit_tries1;
      eh->max_tries2 = desc->xmit_tries2;
      eh->max_tries3 = desc->xmit_tries3;
    }
    q->pull(ATHDESC_HEADER_SIZE);
  }

  eh = WIFI_EXTRA_ANNO(q);

  ath2_h = (struct ath2_header*)(q->data());

  if ( ( eh->magic == WIFI_EXTRA_MAGIC ) && ( eh->flags & WIFI_EXTRA_TX ) ) //TXFEEDBACK
  {
    eh->silence = ath2_h->anno.tx.ts_noise;
    eh->virt_col = ath2_h->anno.tx.ts_virtcol;
    if ( eh->retries < ath2_h->anno.tx.ts_longretry )
      eh->retries = ath2_h->anno.tx.ts_longretry;

    if ( (_cst != NULL) && ((uint8_t)ath2_h->anno.tx.ts_channel_utility != CHANNEL_UTILITY_INVALID) )
      _cst->addHWStat(&(p->timestamp_anno()), ath2_h->anno.tx.ts_channel_utility, 0, 0);

    BRNPacketAnno::set_channel_anno(q, ath2_h->anno.tx.ts_channel); 
  }
  else                                                                      //RX
  {
    bool rx_error = true;
    switch (ath2_h->anno.rx.rs_status) {
      case 0:   //OK
          {
            rx_error = false;
            break; //nothing to do
          }
      case 1:   //CRC
          {
            eh->flags |= WIFI_EXTRA_RX_CRC_ERR;
            break;
          }
      case 2:   //PHY
          {
            eh->flags |= WIFI_EXTRA_RX_PHY_ERR;
            break;
          }
      case 4:  //FIFO
          {
            eh->flags |= WIFI_EXTRA_RX_FIFO_ERR;
            break;
          }
      case 8:  //DECRYPT
          {
            eh->flags |= WIFI_EXTRA_RX_DECRYPT_ERR;
            break;
          }
      case 16: //MIC
          {
            eh->flags |= WIFI_EXTRA_RX_MIC_ERR;
            break;
          }
      default:
          rx_error = false;
    }

    if ( ! rx_error && ( eh->flags & WIFI_EXTRA_RX_PHY_ERR ) ) {
      click_chatter("ERROR mismatch");
#ifndef CLICK_LINUXMODULE
      exit(0);
#endif
    }

    eh->silence = ath2_h->anno.rx.rs_noise;
    eh->power = (uint8_t)((int)ath2_h->anno.rx.rs_noise + (int)eh->rssi);

    BRNPacketAnno::set_channel_anno(q, ath2_h->anno.rx.rs_channel);

    if ( (_cst != NULL) && ((uint8_t)ath2_h->anno.rx.rs_channel_utility != CHANNEL_UTILITY_INVALID) )
      _cst->addHWStat(&(p->timestamp_anno()), ath2_h->anno.rx.rs_channel_utility, 0, 0);
  }


  q->pull(sizeof(struct ath2_header));

  return q;
}

void
Ath2Decap::add_handlers()
{
  BRNElement::add_handlers();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Decap)
ELEMENT_MT_SAFE(Ath2Decap)
