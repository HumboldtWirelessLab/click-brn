#include <click/config.h>
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>

#ifdef HAVE_LIBIW
#include "iwlib.h"
#endif

#include "wificonfig_iwlib.hh"

CLICK_DECLS

#ifdef HAVE_LIBIW
int WifiConfigIwLib::sock_iwconfig = 0;
int WifiConfigIwLib::sock_iwconfig_refs = 0;
#endif

WifiConfigIwLib::WifiConfigIwLib(String dev_name)
{
  device_name = dev_name;
#ifdef HAVE_LIBIW
  if (!sock_iwconfig) sock_iwconfig = iw_sockets_open();

  //TODO: locks?
  sock_iwconfig_refs++;
#endif
}

WifiConfigIwLib::~WifiConfigIwLib()
{
#ifdef HAVE_LIBIW
  sock_iwconfig_refs--;

  if (!sock_iwconfig_refs) {
    if (sock_iwconfig) {
      iw_sockets_close(sock_iwconfig);
      sock_iwconfig = 0;
    }
  }
#endif
}

#ifdef HAVE_LIBIW
int
WifiConfigIwLib::get_info(struct wireless_info *info)
{

    struct iwreq wrq;

    memset((char *)info, 0, sizeof(struct wireless_info));

    /* Get basic information */
    if (iw_get_basic_config(sock_iwconfig, device_name.c_str(), &(info->b)) < 0) {
        /* If no wireless name : no wireless extensions */
        /* But let's check if the interface exists at all */
        struct ifreq ifr;

        strncpy(ifr.ifr_name, device_name.c_str(), IFNAMSIZ);
        if (ioctl(sock_iwconfig, SIOCGIFFLAGS, &ifr) < 0)
            return (-ENODEV);
        else
            return (-ENOTSUP);
    }

    /* Get ranges */
    if (iw_get_range_info(sock_iwconfig, device_name.c_str(), &(info->range)) >= 0)
        info->has_range = 1;

    /* Get AP address */
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWAP, &wrq) >= 0) {
        info->has_ap_addr = 1;
        memcpy(&(info->ap_addr), &(wrq.u.ap_addr), sizeof(sockaddr));
    }

    /* Get bit rate */
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWRATE, &wrq) >= 0) {
        info->has_bitrate = 1;
        memcpy(&(info->bitrate), &(wrq.u.bitrate), sizeof(iwparam));
    }

    /* Get Power Management settings */
    wrq.u.power.flags = 0;
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWPOWER, &wrq) >= 0) {
        info->has_power = 1;
        memcpy(&(info->power), &(wrq.u.power), sizeof(iwparam));
    }

    /* Get stats */
    if (iw_get_stats(sock_iwconfig, device_name.c_str(), &(info->stats),
             &info->range, info->has_range) >= 0) {
        info->has_stats = 1;
    }

    /* Get NickName */
    wrq.u.essid.pointer = (caddr_t) info->nickname;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    wrq.u.essid.flags = 0;
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWNICKN, &wrq) >= 0)
        if (wrq.u.data.length > 1)
            info->has_nickname = 1;

    if ((info->has_range) && (info->range.we_version_compiled > 9)) {
        /* Get Transmit Power */
        if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWTXPOW, &wrq) >= 0) {
            info->has_txpower = 1;
            memcpy(&(info->txpower), &(wrq.u.txpower),
                   sizeof(iwparam));
        }
    }

    /* Get sensitivity */
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWSENS, &wrq) >= 0) {
        info->has_sens = 1;
        memcpy(&(info->sens), &(wrq.u.sens), sizeof(iwparam));
    }

    if ((info->has_range) && (info->range.we_version_compiled > 10)) {
        /* Get retry limit/lifetime */
        if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWRETRY, &wrq) >= 0) {
            info->has_retry = 1;
            memcpy(&(info->retry), &(wrq.u.retry), sizeof(iwparam));
        }
    }

    /* Get RTS threshold */
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWRTS, &wrq) >= 0) {
        info->has_rts = 1;
        memcpy(&(info->rts), &(wrq.u.rts), sizeof(iwparam));
    }

    /* Get fragmentation threshold */
    if (iw_get_ext(sock_iwconfig, device_name.c_str(), SIOCGIWFRAG, &wrq) >= 0) {
        info->has_frag = 1;
        memcpy(&(info->frag), &(wrq.u.frag), sizeof(iwparam));
    }

    return (0);
}
#endif

int
WifiConfigIwLib::get_txpower()
{
#ifdef HAVE_LIBIW
    wireless_info wi;

    if (get_info(&wi) != 0) {
        fprintf(stderr, "Error: get_info\n");
    }

    if (wi.has_txpower) {
        return wi.txpower.value;
    }
#endif

    return -1;
}

int
WifiConfigIwLib::set_txpower(int power)
{
#ifdef HAVE_LIBIW
    struct iwreq wrq;
    struct iw_range range;

    /* Extract range info */
    if (iw_get_range_info(sock_iwconfig, device_name.c_str(), &range) < 0)
        memset(&range, 0, sizeof(range));

    /* Prepare the request */
    wrq.u.txpower.value = -1;
    wrq.u.txpower.fixed = 1;
    wrq.u.txpower.disabled = 0;
    wrq.u.txpower.flags = IW_TXPOW_DBM;

    {
        int ismwatt = 0;

        /* Convert */
        if (range.txpower_capa & IW_TXPOW_RELATIVE) {
            /* Can't convert */
            if (ismwatt)
                ABORT_ARG_TYPE("Set Tx Power", SIOCSIWTXPOW,
                           "10");
        } else {
            if (range.txpower_capa & IW_TXPOW_MWATT) {
                if (!ismwatt)
                    power = iw_dbm2mwatt(power);
                wrq.u.txpower.flags = IW_TXPOW_MWATT;
            } else {
                if (ismwatt)
                    power = iw_mwatt2dbm(power);
                wrq.u.txpower.flags = IW_TXPOW_DBM;
            }
        }
        wrq.u.txpower.value = power;

    }

    IW_SET_EXT_ERR(sock_iwconfig, device_name.c_str(), SIOCSIWTXPOW, &wrq,
               "Set Tx Power");

#else

  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << device_name;
  cmda << " txpower " << power;
  String cmd = cmda.take_string();

  click_chatter("SetPower command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", NULL /*errh*/);
  if (out) click_chatter("%s: %s", cmd.c_str(), out.c_str());

#endif

  return 0;
}

int
WifiConfigIwLib::get_max_txpower()
{
  int saved_txpower = get_txpower();

  int ac_power, max_power;

  set_txpower(100);
  max_power = ac_power = get_txpower();

  do {
        ac_power++;
    set_txpower(ac_power);
    max_power = get_txpower();
    } while (ac_power == max_power);        //loop while setting new txpower was successful

  set_txpower(saved_txpower);

  return max_power;
}

int
WifiConfigIwLib::get_rates(Vector<MCS> &rates)
{
  rates.clear();

  return 0;
}

int
WifiConfigIwLib::get_channel()
{
#ifdef HAVE_LIBIW
#else
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << device_name;
  cmda << " channel " << channel;
  String cmd = cmda.take_string();

  click_chatter("GetChannel command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", NULL /*errh*/);
  if (out) click_chatter("%s: %s", cmd.c_str(), out.c_str());

#endif
  return 0;
}

int
WifiConfigIwLib::set_channel(int channel)
{
#ifdef HAVE_LIBIW
  (void)channel;
#else
  StringAccum cmda;
  if (access("/sbin/iwconfig", X_OK) == 0)
    cmda << "/sbin/iwconfig";
  else if (access("/usr/sbin/iwconfig", X_OK) == 0)
    cmda << "/usr/sbin/iwconfig";
  else
    return 0;

  cmda << " " << device_name;
  cmda << " channel " << channel;
  String cmd = cmda.take_string();

  click_chatter("SetChannel command: %s",cmd.c_str());

  String out = shell_command_output_string(cmd, "", NULL /*errh*/);
  if (out) click_chatter("%s: %s", cmd.c_str(), out.c_str());

#endif

  return 0;
}

ELEMENT_PROVIDES(WifiConfigIwLibIwLib)
ELEMENT_REQUIRES(userlevel|ns)
CLICK_ENDDECLS
