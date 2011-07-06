#ifndef FALCON_LEAVE_ORGANIZER_HH
#define FALCON_LEAVE_ORGANIZER_HH
#include <click/element.hh>

#include "elements/brn2/standard/brn_md5.hh"
#include "falcon_routingtable.hh"
CLICK_DECLS

#define FALCON_LEAVE_MODE_IDLE  0
#define FALCON_LEAVE_MODE_LEAVE 1

class FalconLeaveOrganizer : public Element
{
  public:
    FalconLeaveOrganizer();
    ~FalconLeaveOrganizer();

/*ELEMENT*/
    const char *class_name() const  { return "FalconLeaveOrganizer"; }

    const char *processing() const  { return PUSH; }

    const char *port_count() const  { return "1/1"; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const  { return false; }

    int initialize(ErrorHandler *);

    void push( int port, Packet *packet );

    bool start_leave(md5_byte_t *id, int id_len);
    void handle_leave_request(Packet *packet);
    void handle_leave_reply(Packet *packet);

  private:
    FalconRoutingTable *_frt;
    DHTnodelist rev_list;

    Timer _lookup_timer;
    static void static_lookup_timer_hook(Timer *, void *);

    int _max_retries;
    int _mode;

    md5_byte_t _new_id[MAX_NODEID_LENTGH];
    int _new_id_len;
    int _debug;
};

CLICK_ENDDECLS
#endif
