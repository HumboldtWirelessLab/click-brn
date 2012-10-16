#ifndef CLICK_ATHSETFLAGS_HH
#define CLICK_ATHSETFLAGS_HH
#include <click/element.hh>
#include <clicknet/ether.h>
CLICK_DECLS

#define INVALID_VALUE (0xffffffff)

class AthSetFlags : public Element { public:

  AthSetFlags();
  ~AthSetFlags();

  const char *class_name() const { return "AthSetFlags"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  Packet *simple_action(Packet *);

  void add_handlers();

  bool _debug;

 private:

  uint32_t frame_len;
  uint32_t reserved_12_15;

  uint32_t rts_cts_enable;
  uint32_t veol;
  uint32_t clear_dest_mask;
  uint32_t ant_mode_xmit;
  uint32_t inter_req;
  uint32_t encrypt_key_valid;
  uint32_t cts_enable;

  uint32_t buf_len;
  uint32_t more;
  uint32_t encrypt_key_index;
  uint32_t frame_type;
  uint32_t no_ack;
  uint32_t comp_proc;
  uint32_t comp_iv_len;
  uint32_t comp_icv_len;
  uint32_t reserved_31;

  uint32_t rts_duration;
  uint32_t duration_update_enable;

  uint32_t rts_cts_rate;
  uint32_t reserved_25_31;
};

CLICK_ENDDECLS
#endif
