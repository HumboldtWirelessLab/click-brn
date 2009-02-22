#include <click/config.h>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/packet_anno.hh>
#include <clicknet/llc.h>
#include "elements/wifi/athdesc.h"
#include "athsetflags.hh"

CLICK_DECLS


AthSetFlags::AthSetFlags():
   frame_len(0),
   reserved_12_15(0),
   rts_cts_enable(0),
   veol(0),
   clear_dest_mask(0),
   ant_mode_xmit(0),
   inter_req(0),
   encrypt_key_valid(0),
   cts_enable(0),
   buf_len(0),
   more(0),
   encrypt_key_index(0),
   frame_type(0),
   no_ack(0),
   comp_proc(0),
   comp_iv_len(0),
   comp_icv_len(0),
   reserved_31(0),
   rts_duration(0),
   duration_update_enable(0),
   rts_cts_rate(0),
   reserved_25_31(0)
{
}

AthSetFlags::~AthSetFlags()
{
}

int
AthSetFlags::configure(Vector<String> &conf, ErrorHandler *errh)
{

  _debug = false;
  if (cp_va_kparse(conf, this, errh,
      "FLEN", cpkN, cpInteger, &frame_len,
      "RESERVED", cpkN, cpInteger, &reserved_12_15,
      "RTSCTS", cpkN, cpInteger, &rts_cts_enable,
      "VEOL", cpkN, cpInteger, &veol,
      "CDM", cpkN, cpInteger, &clear_dest_mask,
      "ANTXM", cpkN, cpInteger, &ant_mode_xmit,
      "INTERREQ", cpkN, cpInteger, &inter_req,
      "KEYVAL", cpkN, cpInteger, &encrypt_key_valid,
      "CTS", cpkN, cpInteger, &cts_enable,
      "BLEN", cpkN, cpInteger, &buf_len,
      "MORE", cpkN, cpInteger, &more,
      "KEYIND", cpkN, cpInteger, &encrypt_key_index,
      "FTYPE", cpkN, cpInteger, &frame_type,
      "NOACK", cpkN, cpInteger, &no_ack,
      "COMPPROC", cpkN, cpInteger, &comp_proc,
      "IVLEN", cpkN, cpInteger, &comp_iv_len,
      "ICVLEN", cpkN, cpInteger, &comp_icv_len,
      "RESERVED31", cpkN, cpInteger, &reserved_31,
      "RTSDUR", cpkN, cpInteger, &rts_duration,
      "DURUPD", cpkN, cpInteger, &duration_update_enable,
      "RTSCTSRATE", cpkN, cpInteger, &rts_cts_rate,
      "RESERVED2", cpkN, cpInteger, &reserved_25_31,
      "DEBUG", 0, cpBool, &_debug,
      cpEnd) < 0)
    return -1;
  return 0;
}

Packet *
AthSetFlags::simple_action(Packet *p)
{
  struct ar5212_desc *desc  = (struct ar5212_desc *) (p->data() + 8);

  desc->frame_len = frame_len;
  desc->reserved_12_15 = reserved_12_15;
  desc->rts_cts_enable = rts_cts_enable;
  desc->veol = veol;
  desc->clear_dest_mask = clear_dest_mask;
  desc->ant_mode_xmit = ant_mode_xmit;
  desc->inter_req = inter_req;
  desc->encrypt_key_valid = encrypt_key_valid;
  desc->cts_enable = cts_enable;

  desc->buf_len = buf_len;
  desc->more = more;
  desc->encrypt_key_index = encrypt_key_index;
  desc->frame_type = frame_type;
  desc->no_ack = no_ack;
  desc->comp_proc = comp_proc;
  desc->comp_iv_len = comp_iv_len;
  desc->comp_icv_len = comp_icv_len;
  desc->reserved_31 = reserved_31;

  desc->rts_duration = rts_duration;
  desc->duration_update_enable = duration_update_enable;

  desc->rts_cts_rate = rts_cts_rate;
  desc->reserved_25_31 = reserved_25_31;

  return p;
}


enum {H_DEBUG};

static String 
AthSetFlags_read_param(Element *e, void *thunk)
{
  AthSetFlags *td = (AthSetFlags *)e;
    switch ((uintptr_t) thunk) {
      case H_DEBUG:
        return String(td->_debug) + "\n";
      default:
        return String();
    }
}
static int 
AthSetFlags_write_param(const String &in_s, Element *e, void *vparam, ErrorHandler *errh)
{
  AthSetFlags *f = (AthSetFlags *)e;
  String s = cp_uncomment(in_s);
  switch((intptr_t)vparam) {
  case H_DEBUG: {    //debug
    bool debug;
    if (!cp_bool(s, &debug)) 
      return errh->error("debug parameter must be boolean");
    f->_debug = debug;
    break;
  }
  }
  return 0;
}

void
AthSetFlags::add_handlers()
{
  add_read_handler("debug", AthSetFlags_read_param, (void *) H_DEBUG);
  add_write_handler("debug", AthSetFlags_write_param, (void *) H_DEBUG);
}
CLICK_ENDDECLS
EXPORT_ELEMENT(AthSetFlags)
