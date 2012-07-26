/*
 * tosimdevice.{cc,hh} -- writes packets to simulated network devices
 *
 */

/*****************************************************************************
 *  Copyright 2002, Univerity of Colorado at Boulder                         *
 *  Copyright (c) 2006 Regents of the University of California               *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           *
 ****************************************************************************/


#include <click/config.h>
#include "fromsimdevice.hh"
#include "tosimdevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <stdio.h>
#include <unistd.h>

#include <clicknet/wifi.h>
CLICK_DECLS

ToSimDevice::ToSimDevice()
  : _packets_in_sim_queue(0), _fd(-1), _my_fd(false), _task(this), _encap_type(SIMCLICK_PTYPE_ETHER),
    _polling(true), _txfeedback_anno(false)
{
}

ToSimDevice::~ToSimDevice()
{
  uninitialize();
}

int
ToSimDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  String encap_type;
  if (Args(conf, this, errh)
      .read_mp("DEVNAME", _ifname)
      .read_p("ENCAP", WordArg(), encap_type)
      .read_p("POLLING", BoolArg(), _polling)
      .read_p("HAVETXFEEDBACKANNO", BoolArg(), _txfeedback_anno)
      .complete() < 0)
    return -1;
  if (!_ifname)
    return errh->error("interface not set");
  if (!encap_type || encap_type == "ETHER")
    _encap_type = SIMCLICK_PTYPE_ETHER;
  else if (encap_type == "IP")
    _encap_type = SIMCLICK_PTYPE_IP;
  else if (encap_type == "UNKNOWN")
      _encap_type = SIMCLICK_PTYPE_UNKNOWN;
  else
    return errh->error("bad encapsulation type");

  return 0;
}

int
ToSimDevice::initialize(ErrorHandler *errh)
{
  _fd = -1;
  if (!_ifname)
    return errh->error("interface not set");

  // Get the simulator ifid
  Router* myrouter = router();
  _fd = myrouter->sim_get_ifid(_ifname.c_str());
  if (_fd < 0) return -1;

  _my_fd = true;
  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }

  if ( ! _polling ) {
    // Request that we get packets sent to us from the simulator
    myrouter->sim_listen(_fd,eindex());
  }

  return 0;
}

void
ToSimDevice::uninitialize()
{
  _task.unschedule();
}

void
ToSimDevice::send_packet(Packet *p)
{
  Router* myrouter = router();
  int retval;
  // We send out either ethernet or IP
  retval = myrouter->sim_write(_fd,_encap_type,p->data(),p->length(),
				 p->get_sim_packetinfo());
  p->kill();
}

void
ToSimDevice::push(int, Packet *p)
{
  assert(p->length() >= 14);
  //fprintf(stderr,"Hey!!! Pushing!!!\n");
  send_packet(p);
}

int
ToSimDevice::incoming_packet(int /*ifid*/, int /*ptype*/, const unsigned char* data, int /*len*/,
                        simclick_simpacketinfo* pinfo)
{
  bool feedback = false;

  if ( _txfeedback_anno ) {
    feedback = (pinfo->txfeedback == 1);
  } else {
    struct click_wifi_extra *ceh = (struct click_wifi_extra *)data;

    feedback = (ceh->magic == WIFI_EXTRA_MAGIC && ceh->flags & WIFI_EXTRA_TX );
  }

  if ( feedback ) {
    _packets_in_sim_queue--;

    if ( (_packets_in_sim_queue == 0) && (!router()->sim_if_ready(_fd)) )
      click_chatter("ERROR: SimDev not ready, but also no packets in sim dev queue");

    if ( _signal.active() ) {
      _task.reschedule();
    }
  }
  
  return 0;
}

bool
ToSimDevice::run_task(Task *)
{
  // XXX reduce tickets when idle
  bool active = false;
  if (router()->sim_if_ready(_fd)) {
    //fprintf(stderr,"Hey!!! Pulling ready!!!\n");
    if (Packet *p = input(0).pull()) {
      //fprintf(stderr,"Hey!!! Sending a packet!!!\n");
      _packets_in_sim_queue++;
      send_packet(p);
      active = true;
    }
  }

  // don't reschedule if no packets upstream
  if ( (_polling && (active || _signal.active())) ||
       (!_polling && (_packets_in_sim_queue == 0 ) && _signal.active()) )
    _task.fast_reschedule();

  return active;
}

void
ToSimDevice::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromSimDevice ns)
EXPORT_ELEMENT(ToSimDevice)
