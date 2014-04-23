/*
 * simdevice.{cc,hh} -- simulated network devices
 *
 */

#include <click/config.h>
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>

#include <stdio.h>
#include <unistd.h>
#include <click/userutils.hh>

#include "elements/brn/standard/brnlogger/brnlogger.hh"
#include <elements/wifi/bitrate.hh>
#include <elements/brn/wifi/brnwifi.hh>
#include "simdevice.hh"

CLICK_DECLS

static int rx_range[] = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, -1 };
static int snr_offset[] = { 10, 30, 20, 35, 55, 40, 60, 45, 65, 50, 72, 55, 90, 60, 110, 65, 120, 70, 130, 75, 135, 80, 144, 85, 150, 90, 180, 95, 195, 100, 217, 105, 240, 110, 260, 115, 270, 120, 288, 125, 289, 130, 300, 135, 360, 140, 390, 145, 405, 150, 433, 155, 434, 160, 450, 165, 480, 170, 520, 175, 540, 180, 578, 185, 585, 190, 600, 195, 650, 200, 722, 205, 780, 210, 810, 215, 866, 220, 900, 225, 1040, 230, 1080, 235, 1156, 240, 1170, 245, 1200, 250, 1215, 255, 1300, 260, 1350, 265, 1444, 270, 1500, 275, 1620, 280, 1800, 285, 2160, 290, 2400, 295, 2430, 300, 2700, 305, 3000, 310, -1, -1 };

SimDevice::SimDevice()
  : _task(this),
    empirical_index(0),
    empirical_packet_count(100)
{
  BRNElement::init();
}

SimDevice::~SimDevice()
{
  uninitialize();
}

int
SimDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (Args(conf, this, errh)
      .read_mp("DEVNAME", _ifname)
      .read("EMPERICAL_FILE", _empirical_file)
      .complete() < 0)
    return -1;

  if (!_ifname)
    return errh->error("interface not set");

  if (_empirical_file) {
    empirical_data_ht_size = NO_HT_RATES * empirical_packet_count;     //MCS-Index: 16 BW: 2 SGI: 2

    empirical_data_ht = new int[empirical_data_ht_size];
    empirical_psr_ht = new uint16_t[NO_HT_RATES];

    memset(empirical_data_ht, 0, empirical_data_ht_size * sizeof(int));
    memset(empirical_psr_ht, 0, NO_HT_RATES * sizeof(uint16_t));

    empirical_data_non_ht_size = NO_NON_HT_RATES * empirical_packet_count;  //

    empirical_data_non_ht = new int[empirical_data_non_ht_size];
    empirical_psr_non_ht = new uint16_t[NO_NON_HT_RATES];

    memset(empirical_data_non_ht, 0, empirical_data_non_ht_size * sizeof(int));
    memset(empirical_psr_non_ht, 0, NO_NON_HT_RATES * sizeof(uint16_t));
  }

  return 0;
}

static int
get_index(int rate)
{
  switch (rate) {
    case 10: return 0;
    case 20: return 1;
    case 55: return 2;
    case 110: return 3;
    case 60: return 4;
    case 90: return 5;
    case 120: return 6;
    case 180: return 7;
    case 240: return 8;
    case 360: return 9;
    case 480: return 10;
    case 540: return 11;
  }

  click_chatter("Unknown rate: %d",rate);

  return -1;
}

static int abg_rates[] = { 10, 20, 55, 110, 60, 90, 120, 180, 240, 360, 480, 540 };
static char abg_mode[] = { 'b', 'b', 'b', 'b', 'g', 'g', 'g', 'g', 'g', 'g', 'g', 'g' };

int
SimDevice::initialize(ErrorHandler *errh)
{
  click_brn_srandom();

  for ( _rx_range_size = 0; rx_range[_rx_range_size] != -1; _rx_range_size++);

  if (!_ifname)
    return errh->error("interface not set");

  if (_empirical_file) {
    String emp_data_string = file_string(_empirical_file);
    //click_chatter("%s",emp_data_string.c_str());

    Vector<String> emp_data;

    cp_spacevec(emp_data_string, emp_data);

    for ( int i = 0; i < emp_data.size(); i +=8 ) {
      int rate, rate_is_ht, rate_index, ht40, sgi, rssi, seq;

      cp_integer(emp_data[i],&rate);
      cp_integer(emp_data[i+1],&rate_is_ht);
      cp_integer(emp_data[i+2],&rate_index);
      cp_integer(emp_data[i+3],&ht40);
      cp_integer(emp_data[i+4],&sgi);
      cp_integer(emp_data[i+5],&rssi);
      cp_integer(emp_data[i+7],&seq);

      if ( (rssi == 0) || (rssi > 100) ) rssi = 255;

      if ( rate_is_ht == 1 ) {
        empirical_data_ht[100 * ( 4 * rate_index + 2 * ht40 + sgi ) + seq] = rssi;
      } else {
        empirical_data_non_ht[100 * get_index(rate) + seq] = rssi;
      }
    }
  }

  /* Calc PSR */
  for ( int i = 0; i < NO_HT_RATES; i++ ) {
    int rx_pkt = 0;
    for ( int p = 0; p < 100; p++) {
      if (empirical_data_ht[(100 * i) + p] != 0) rx_pkt++;
    }
    empirical_psr_ht[i] = rx_pkt;
  }

  for ( int i = 0; i < NO_NON_HT_RATES; i++ ) {
    int rx_pkt = 0;
    for ( int p = 0; p < 100; p++) {
      if (empirical_data_non_ht[(100 * i) + p] != 0) rx_pkt++;
    }
    empirical_psr_non_ht[i] = rx_pkt;
  }


  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }

  return 0;
}

void
SimDevice::uninitialize()
{
  _task.unschedule();
}

Packet*
SimDevice::sim_packet_transmission(Packet *p)
{
  struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);

  uint8_t rate_is_ht, rate_index, rate_bw, rate_sgi;
  uint16_t rate = 0;

  int pro = 0, tries = 0;
  int snr = (int)ceh->power * 10;

  int rates_field[4], tries_field[4];

  //click_chatter("Rates: %d (%d)  %d (%d)  %d (%d)  %d (%d)",ceh->rate, ceh->max_tries, ceh->rate1, ceh->max_tries1,
  //                                                          ceh->rate2, ceh->max_tries2, ceh->rate3, ceh->max_tries3);

  rates_field[0] = ceh->rate; rates_field[1] = ceh->rate1; rates_field[2] = ceh->rate2; rates_field[3] = ceh->rate3;

  tries_field[0] = ceh->max_tries; tries_field[1] = ceh->max_tries1;
  tries_field[2] = ceh->max_tries2; tries_field[3] = ceh->max_tries3;


  for ( int r_i = 0; r_i < 4; r_i++ ) {
    for ( int i = 0; i < (int)tries_field[r_i]; i++ ) {
      if ( BrnWifi::getMCS(ceh,r_i) == 1) {
        BrnWifi::toMCS(&rate_index, &rate_bw, &rate_sgi, rates_field[r_i]);
        rate = BrnWifi::getMCSRate(rate_index, rate_bw, rate_sgi);
        rate_is_ht = 1;
      } else {
        rate = 5 * rates_field[r_i];
        rate_is_ht = 0;
      }

      if ( _empirical_file ) {
        int rssi = 0;

        if ( rate_is_ht == 1 ) {
          rssi = empirical_data_ht[100 * ( 4 * (int)rate_index + 2 * (int)rate_bw + (int)rate_sgi ) + empirical_index];
        } else {
          rssi = empirical_data_non_ht[100 * get_index(rate) + empirical_index];
        }

        empirical_index = ( empirical_index + 1 ) % empirical_packet_count;

        if ( rssi > 0 ) {
          ceh->retries = tries;
          ceh->flags |= WIFI_EXTRA_TX;
          ceh->silence = (uint8_t)-95;
          if ( rssi == 255 ) ceh->rssi = 0;
          else ceh->rssi = rssi;

          //if ( r_i > 0 ) ceh->flags |= WIFI_EXTRA_TX_USED_ALT_RATE;
          return p;
        }
      } else {
        int r = click_random() % 100;
        pro = get_rx_probability(rate, snr);

        if ( r <= pro ) {
          ceh->retries = tries;
          ceh->flags |= WIFI_EXTRA_TX;
          ceh->silence = (uint8_t)-95;
          ceh->rssi = snr;
          //if ( r_i > 0 ) ceh->flags |= WIFI_EXTRA_TX_USED_ALT_RATE;
          return p;
        }
      }
      tries++;
    }
  }

  ceh->retries = tries;
  ceh->flags |= WIFI_EXTRA_TX;
  ceh->flags |= WIFI_EXTRA_TX_FAIL;
  //ceh->flags |= WIFI_EXTRA_TX_USED_ALT_RATE;

  return p;
}

int
SimDevice::get_rx_probability(int rate, int snr)
{
  int snr_eff = snr;

  int i = 0, p;

  while ( snr_offset[i] != -1 ) {
    if ( snr_offset[i] == rate ) {
      snr_eff -= snr_offset[i+1];

      if ( snr_eff < 0 ) {
        return 0;
      }

      for ( p = _rx_range_size - 1; p >= 0; p--) {
        if ( snr_eff >= rx_range[p] ) break;
      }

      if ( p < 0 ) return 0;

      return (100 * p) / (_rx_range_size-1);
    }
    i += 2;
  }

  click_chatter("Rate %d not found.",rate);

  return 0;
}



void
SimDevice::push(int, Packet *p)
{
  assert(p->length() >= 14);
  Packet *p_out = sim_packet_transmission(p);

  if ( p_out ) output(0).push(p_out);
}

bool
SimDevice::run_task(Task *)
{
  // XXX reduce tickets when idle
  bool active = false;

  if (Packet *p = input(0).pull()) {
    Packet *p_out = sim_packet_transmission(p);
    if ( p_out ) output(0).push(p_out);
    active = true;
  }

  // don't reschedule if no packets upstream
  if (active || _signal.active())
    _task.fast_reschedule();

  return active;
}

/*************************************************************************/
/************************ H A N D L E R **********************************/
/*************************************************************************/

String
SimDevice::print_psr()
{
  StringAccum sa;

  sa << "<simdev>\n\t<rates>\n\t\t<htrates>\n";

/*for ( int i = 0; i < NO_HT_RATES; i++ ) {
    sa << "\t\t\t<htrate index=\"" << i << "\" psr=\"" << empirical_psr_ht[i] << "\" />\n";
  }*/

  for ( int rate_bw = 0; rate_bw <= 1; rate_bw++ ) {
    for ( int rate_sgi= 0; rate_sgi <= 1; rate_sgi++ ) {
      sa << "\t\t\t<htrate mode=\"n\" ht40=\"" << rate_bw << "\" sgi=\"" << rate_sgi << "\" >\n";
      for ( int rate_index = 0; rate_index < 16; rate_index++ ) {
        uint32_t rate = BrnWifi::getMCSRate(rate_index, rate_bw, rate_sgi);
        uint32_t psr = empirical_psr_ht[( 4 * (int)rate_index + 2 * (int)rate_bw + (int)rate_sgi )];
        sa << "\t\t\t\t<rate mcs_index=\"" << rate_index << "\" rate=\"" << rate << "\" psr=\"" << psr;
        sa << "\" achievable_rate=\"" << (rate * psr)  << "\" unit=\"kbit/s\" />\n";
      }
      sa << "\t\t\t</htrate>\n";
    }
  }

  sa << "\t\t</htrates>\n\t\t<nonhtrates>\n";

  for ( int i = 0; i < NO_NON_HT_RATES; i++ ) {
    uint32_t psr = empirical_psr_non_ht[i];
    sa << "\t\t\t<rate mode=\"" << abg_mode[i] << "\" ht40=\"0\" sgi=\"0\" mcs_index=\"0\" rate=\"";
    sa << abg_rates[i] << "\" psr=\"" << psr << "\" achievable_rate=\"" << (abg_rates[i] * psr) << "\" unit=\"kbit/s\" />\n";
  }

  sa << "\t\t</nonhtrates>\n\n\t</rates>\n</simdev>\n";

  return sa.take_string();
}

enum { H_PSR };

static String
read_handler(Element *e, void *thunk)
{
  switch ((uintptr_t) thunk) {
    case H_PSR: return ((SimDevice*)e)->print_psr();
  }
  return String();
}


void
SimDevice::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);

  BRNElement::add_handlers();

  add_read_handler("psr", read_handler, (void *)H_PSR);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(SimDevice)
