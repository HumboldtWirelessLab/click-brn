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
   frame_len(INVALID_VALUE),
   reserved_12_15(INVALID_VALUE),
   rts_cts_enable(INVALID_VALUE),
   veol(INVALID_VALUE),
   clear_dest_mask(INVALID_VALUE),
   ant_mode_xmit(INVALID_VALUE),
   inter_req(INVALID_VALUE),
   encrypt_key_valid(INVALID_VALUE),
   cts_enable(INVALID_VALUE),
   buf_len(INVALID_VALUE),
   more(INVALID_VALUE),
   encrypt_key_index(INVALID_VALUE),
   frame_type(INVALID_VALUE),
   no_ack(INVALID_VALUE),
   comp_proc(INVALID_VALUE),
   comp_iv_len(INVALID_VALUE),
   comp_icv_len(INVALID_VALUE),
   reserved_31(INVALID_VALUE),
   rts_duration(INVALID_VALUE),
   duration_update_enable(INVALID_VALUE),
   rts_cts_rate(INVALID_VALUE),
   reserved_25_31(INVALID_VALUE)
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

  if ( frame_len != INVALID_VALUE ) desc->frame_len = frame_len;
  if ( reserved_12_15 != INVALID_VALUE ) desc->reserved_12_15 = reserved_12_15;
  if ( rts_cts_enable != INVALID_VALUE ) desc->rts_cts_enable = rts_cts_enable;
  if ( veol != INVALID_VALUE ) desc->veol = veol;
  if ( clear_dest_mask != INVALID_VALUE ) desc->clear_dest_mask = clear_dest_mask;
  if ( ant_mode_xmit != INVALID_VALUE ) desc->ant_mode_xmit = ant_mode_xmit;
  if ( inter_req != INVALID_VALUE ) desc->inter_req = inter_req;
  if ( encrypt_key_valid != INVALID_VALUE ) desc->encrypt_key_valid = encrypt_key_valid;
  if ( cts_enable != INVALID_VALUE ) desc->cts_enable = cts_enable;

  if ( buf_len != INVALID_VALUE ) desc->buf_len = buf_len;
  if ( more != INVALID_VALUE ) desc->more = more;
  if ( encrypt_key_index != INVALID_VALUE ) desc->encrypt_key_index = encrypt_key_index;
  if ( frame_type != INVALID_VALUE ) desc->frame_type = frame_type;
  if ( no_ack != INVALID_VALUE ) desc->no_ack = no_ack;
  if ( comp_proc != INVALID_VALUE ) desc->comp_proc = comp_proc;
  if ( comp_iv_len != INVALID_VALUE ) desc->comp_iv_len = comp_iv_len;
  if ( comp_icv_len != INVALID_VALUE ) desc->comp_icv_len = comp_icv_len;
  if ( reserved_31 != INVALID_VALUE ) desc->reserved_31 = reserved_31;

  if ( rts_duration != INVALID_VALUE ) desc->rts_duration = rts_duration;
  if ( duration_update_enable != INVALID_VALUE ) desc->duration_update_enable = duration_update_enable;

  if ( rts_cts_rate != INVALID_VALUE ) desc->rts_cts_rate = rts_cts_rate;
  if ( reserved_25_31 != INVALID_VALUE ) desc->reserved_25_31 = reserved_25_31;

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
