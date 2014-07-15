#ifndef VASERVER_HH
#define VASERVER_HH

#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/ipaddress.hh>
#ifdef HAVE_IP6
# include <click/ip6address.hh>
#endif
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/bighashmap.hh>
#include <click/timer.hh>

#include "elements/brn/brnelement.hh"
#include "elements/brn/ip/linux_rt_ctrl.hh"
#include "csi_protocol.h"

CLICK_DECLS

#define VIRTUEL_ANTENNA_STRATEGY_MAX_MIN_RSSI 0
#define VIRTUAL_ANTENNA_STRATEGY_MAX_THROUGHPUT 1

/*
MCS Index      Data rate               SNR Threshold
0  BPSK 1/2      6.5                     5
1  QPSK 1/2      13                      7
2  QPSK 3/4      19.5                    9
3  16-QAM 1/2    26                      12
4  16-QAM 3/4    39                      15
5  64-QAM 2/3    52                      20
6  64-QAM 3/4    58.5                    22
7  64-QAM 5/6    65                      24
*/

static const int32_t mcs_to_snr_threshold[8] = { 5000, 7000, 9000, 12000, 15000, 20000, 22000, 24000 };
static const int32_t mcs_to_data_rate_kbps[8] = { 6500, 13000, 19500, 26000, 39000, 52000, 58500, 65000 };

/*
 * AP-Selection
 * - Best
 * - Propotional best (je besser desto mehr) (um AP nicht verhungern zu lassen)
 *
 * Rate-Selection (AP (Wahl central))
 * - internal (Open-loop)
 * - Closed Loop
 */

class VAServer : public BRNElement {

  class CSI {
   public:
    IPAddress _from;
    IPAddress _to;

    Timestamp _last_update;

    uint32_t _eff_snrs[MAX_NUM_RATES][4];

    CSI(IPAddress &from, IPAddress &to, uint32_t eff_snrs[MAX_NUM_RATES][4]): _from(from), _to(to) {
      update_csi(eff_snrs);
    }

    void update_csi(uint32_t esnr[MAX_NUM_RATES][4]) {
      memcpy(_eff_snrs, esnr, sizeof(_eff_snrs));
    }
  };

  typedef HashMap<IPAddress, CSI*> CSIMap;
  typedef CSIMap::const_iterator CSIMapIter;

  class VAAntennaInfo {
   public:
    IPAddress _ip;

    VAAntennaInfo(IPAddress ip): _ip(ip) {
    }

    ~VAAntennaInfo() {
    }
  };

  class VAClientInfo {
   public:
    IPAddress _ip;

    CSIMap csi_map;

    VAAntennaInfo *current_antenna;

    struct rtentry routing_entry;

    VAClientInfo(IPAddress ip) : _ip(ip) {
      csi_map.clear();
      current_antenna = NULL;

      IPAddress bcast_ip = IPAddress(255);
      IPAddress default_ip = IPAddress();

      LinuxRTCtrl::set_rt_entry(&routing_entry, _ip, bcast_ip, default_ip);
    }

    void set_gateway(VAAntennaInfo *gw) {
      current_antenna = gw;
      LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry.rt_gateway), gw->_ip);
    }

    void set_mask(IPAddress &mask) {
      LinuxRTCtrl::set_sockaddr((struct sockaddr_in*)&(routing_entry.rt_genmask), mask);
    }
  };

   typedef HashMap<IPAddress, VAAntennaInfo*> VAInfoMap;
   typedef VAInfoMap::const_iterator VAInfoMapIter;

   typedef HashMap<IPAddress, VAClientInfo*> VAClientMap;
   typedef VAClientMap::const_iterator VAClientMapIter;


  public:
    //
    //methods
    //
    VAServer();
    ~VAServer();

    const char *class_name() const  { return "VAServer"; }
    const char *port_count() const  { return "1/0"; }
    const char *processing() const  { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    bool can_live_reconfigure() const { return false; }

    int initialize(ErrorHandler *);
    void run_timer(Timer *);
    void add_handlers();

    void push( int port, Packet *packet );

    void add_csi(IPAddress csi_node, IPAddress client, uint32_t eff_snrs_int[MAX_NUM_RATES][4]);
    IPAddress* select_ap(IPAddress client); //return an ap for an client depending on a scheme

    void set_ap(IPAddress &client, IPAddress &ap);

    String stats();

  private:

    VAInfoMap va_ant_info_map;
    VAClientMap va_cl_info_map;

    uint32_t _strategy;
    LinuxRTCtrl *_rtctrl;

    Timer _rt_update_timer;
    uint32_t _rt_update_interval;

    bool _verbose;
};

CLICK_ENDDECLS
#endif
