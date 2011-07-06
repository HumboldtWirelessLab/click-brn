#ifndef SIMDEVICE_HH
#define SIMDEVICE_HH
#include <click/element.hh>
#include <click/string.hh>
#include <click/task.hh>
#include <click/notifier.hh>

#include "elements/brn2/brnelement.hh"

CLICK_DECLS

class SimDevice : public BRNElement {
 public:

  SimDevice();
  ~SimDevice();

  const char *class_name() const { return "SimDevice"; }
  const char *port_count() const { return PORTS_1_1; }
  const char *processing() const { return AGNOSTIC; }

  int configure_phase() const { return CONFIGURE_PHASE_DEFAULT; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  String ifname() const { return _ifname; }

  void push(int port, Packet *);
  bool run_task(Task *);

  private:

    String _ifname;

    Task _task;

    NotifierSignal _signal;

    int get_rx_probability(int rate, int snr);

    Packet *sim_packet_transmission(Packet *);

    int _rx_range_size;

    String _empirical_file;

    int *empirical_data_ht;
    int empirical_data_ht_size;

    int *empirical_data_non_ht;
    int empirical_data_non_ht_size;

    int empirical_index;

    int empirical_packet_count;
};

CLICK_ENDDECLS
#endif
