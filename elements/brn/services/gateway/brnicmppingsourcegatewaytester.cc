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
 * icmpingsourcegatewaytester.{cc,hh} -- 
 *
 * This element examines a gateway metric by generating ICMP ping and pong packets
 * In this element we use the average latency to an given IP address
 *
 * It calls the Gateway to insert the gateway metric
 *
 */

#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/vector.hh>

#include "brnicmppingsourcegatewaytester.hh"

CLICK_DECLS

/*
 * BRNICMPPingSourceGatewayTester
 *
 */

BRNICMPPingSourceGatewayTester::BRNICMPPingSourceGatewayTester(): _gw(NULL), _old_metric(0) {
    // we have no connection at startup
}

BRNICMPPingSourceGatewayTester::~BRNICMPPingSourceGatewayTester() {}

int
BRNICMPPingSourceGatewayTester::configure(Vector<String> &conf, ErrorHandler *errh) {
    // extract first String of conf for this element
    Vector<String> conf_for_this = conf;
    conf_for_this.erase(conf_for_this.begin()+1, conf_for_this.end());

    // and parse it
    if (cp_va_kparse(conf_for_this, this, errh,
                    "BRNGATEWAY", cpkP+cpkM, cpElement, &_gw,
                    cpEnd) < 0)
        return -1;

    if (_gw->cast("BRNGateway") == 0) {
        return errh->error("No element of type BRNGateway specified.");
    }

    // remove first item
    conf.erase(conf.begin());
    // and pass the remaining conf to the base class
    return ICMPPingSource::configure(conf, errh);
}

void * BRNICMPPingSourceGatewayTester::cast(const char *name) {
    if (strcmp(name, "BRNICMPPingSourceGatewayTester") == 0)
        return dynamic_cast<BRNICMPPingSourceGatewayTester *>(this);
    else if (strcmp(name, "ICMPPingSource") == 0)
        return dynamic_cast<ICMPPingSource *>(this);
    else
        return ICMPPingSource::cast(name);
}

/*
 * We update our metric, when we receive a packet
 * and pass the packet handling to our base class
 *
 */
void
BRNICMPPingSourceGatewayTester::push(int port, Packet *p) {
    // TODO
    // - expoential aging to count more on the recent than the past
    // - What happens, if no packets are received anymore => metric should change to 0 immediately
    // - At the moment I do not count for lost packets, which I probably better should
    // - Reset ReiceiverInfo's values after setting gateway

    // A gateway metric is a value between 0 (worst) and 255 (best)


    /*
    int metric = (this->_receiver->time_sum / (this->_receiver->nreceived ? this->_receiver->nreceived : 1));
    // adjust metric to uint8_t
    if (metric > 1000000) // latency bigger than 1s
        metric = 1000000;
    */

    int metric = 0;

    // normalize metric to values between 0 and 255
    metric = 255 - metric / 3920;


    // we write only significant change of a metric
    // - there must be a change
    // - this change must be a delta of 5
    // - a metric of 0 must be written
    if ((_old_metric != metric) && ((metric == 0) || (_old_metric - metric >= 5))) {
        click_chatter("Significant change in metric (old: %u; new: %u). I try setting it.", _old_metric, metric);

        // set new metric and update saved metric
        if (_gw->add_gateway(BRNGatewayEntry(IPAddress(), metric, false)) == 0)
            _old_metric = metric;
        else
            click_chatter("This is strange. I could not set the gateway metric.");
    }


    ICMPPingSource::push(port, p);
}

EXPORT_ELEMENT(BRNICMPPingSourceGatewayTester)
#include <click/vector.cc>
CLICK_ENDDECLS
