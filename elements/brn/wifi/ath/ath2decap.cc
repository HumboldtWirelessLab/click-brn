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
#include <elements/brn/wifi/brnwifi.h>

#include "ath2decap.hh"
#include "ath2_desc.h"

CLICK_DECLS

Ath2Decap::Ath2Decap()
{
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
                     cpEnd);
  return ret;
}

Packet *
Ath2Decap::simple_action(Packet *p)
{
  WritablePacket *q;
  click_wifi_extra *eh;
  struct ath2_header *ath2_h = NULL;

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
      eh->rssi = rx_desc->rx_rssi;
      if (!rx_desc->rx_ok) {
        eh->flags |= WIFI_EXTRA_RX_ERR;
      }
    } else {
      eh->flags |= WIFI_EXTRA_TX;
      /* tx */
      eh->power = desc->xmit_power;
      eh->rssi = desc->ack_sig_strength;
      eh->rate = ratecode_to_dot11(desc->xmit_rate0);
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
  }
  else                                                                      //RX
  {
    switch (ath2_h->anno.rx.rs_status) {
      //case 0:   //OK
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
      //case 4:  //FIFO
      //case 8:  //DECRYPT
      //case 16: //MIC
    }
    eh->silence = ath2_h->anno.rx.rs_noise;
    eh->power = eh->silence + eh->rssi;
  }

  q->pull(sizeof(struct ath2_header));

  return q;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Ath2Decap)
ELEMENT_MT_SAFE(Ath2Decap)
