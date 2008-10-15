#ifndef BRN2LINKSTATELEMENT_HH
#define BRN2LINKSTATELEMENT_HH

#include <click/element.hh>
#include <click/timer.hh>

CLICK_DECLS

/*
 * =c
 * BRN2Linkstat()
 * =s
 * 
 * =d
 */

class BRN2_LinkstatInfo {
  public:
    EtherAddress _node_address;
    Vector<uint32_t> _linkprobe_squen;
    struct timeval _last_updated;
    uint32_t _max_sequnr;

    BRN2_LinkstatInfo()
    {}

    BRN2_LinkstatInfo(EtherAddress node_address, uint32_t sequencenr, uint32_t max_sequnr)
    {
      _node_address = node_address;
      click_gettimeofday(&_last_updated);
      _linkprobe_squen.push_back(sequencenr);
      _max_sequnr = max_sequnr;
    }

    void update_linkstatInfo(uint32_t sequencenr)
    {
      uint32_t last_index;

      last_index = _linkprobe_squen.size() - 1;

      if ( sequencenr < _linkprobe_squen[last_index] )
      {
        //restart ??
        //_linkprobe_sequen.erase(0, (_linkstate_packets.size()-1));
        if ( _linkprobe_squen.size() > 0 ) click_chatter("Could not clear vector");
      }

       _linkprobe_squen.push_back(sequencenr);

       last_index = _linkprobe_squen.size();

       while ( ( sequencenr - _linkprobe_squen[0] ) >= _max_sequnr )
       {
         _linkprobe_squen.erase(_linkprobe_squen.begin());
       }
    }
};

class BRN2Linkstat : public Element {

 public:

  int _debug;
  //
  //methods
  //

/* brn2_linkstat.cc**/

  BRN2Linkstat();
  ~BRN2Linkstat();

  const char *class_name() const  { return "BRN2Linkstat"; }
  const char *processing() const  { return PUSH; }
  const char *port_count() const  { return "1/1"; }

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const  { return false; }

  void push( int port, Packet *packet );

  void run_timer(Timer *t);

  int initialize(ErrorHandler *);
  void add_handlers();

  Vector<EtherAddress> my_neighbors;
  Vector<BRN2_LinkstatInfo> neighbors_ls_info;
  EtherAddress _me;

  uint32_t _interval_size;

 private:

  uint32_t _interval;
  uint32_t _size;

  uint32_t _sequencenumber;

  Timer _timer;

  Packet *create_linkprobe(int size);
};

CLICK_ENDDECLS
#endif
