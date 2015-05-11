#ifndef WIFI_CONFIG_IWLIB_HH
#define WIFI_CONFIG_IWLIB_HH

#include "wificonfig.hh"

CLICK_DECLS

#ifdef HAVE_LIBIW

/************************* SETTING ROUTINES **************************/

/*------------------------------------------------------------------*/
/*
 * Macro to handle errors when setting WE
 * Print a nice error message and exit...
 * We define them as macro so that "return" do the right thing.
 * The "do {...} while(0)" is a standard trick
 */
#define ERR_SET_EXT(rname, request) \
    fprintf(stderr, "Error for wireless request \"%s\" (%X) :\n", \
        rname, request)

#define ABORT_ARG_NUM(rname, request) \
    do { \
        ERR_SET_EXT(rname, request); \
        fprintf(stderr, "    too few arguments.\n"); \
        return(-1); \
    } while(0)

#define ABORT_ARG_TYPE(rname, request, arg) \
    do { \
        ERR_SET_EXT(rname, request); \
        fprintf(stderr, "    invalid argument \"%s\".\n", arg); \
        return(-2); \
    } while(0)

#define ABORT_ARG_SIZE(rname, request, max) \
    do { \
        ERR_SET_EXT(rname, request); \
        fprintf(stderr, "    argument too big (max %d)\n", max); \
        return(-3); \
    } while(0)

/*------------------------------------------------------------------*/
/*
 * Wrapper to push some Wireless Parameter in the driver
 * Use standard wrapper and add pretty error message if fail...
 */
#define IW_SET_EXT_ERR(skfd, ifname, request, wrq, rname) \
    do { \
    if(iw_set_ext(skfd, ifname, request, wrq) < 0) { \
        ERR_SET_EXT(rname, request); \
        fprintf(stderr, "    SET failed on device %-1.16s ; %s.\n", \
            ifname, strerror(errno)); \
        return(-5); \
    } } while(0)

/*------------------------------------------------------------------*/
/*
 * Wrapper to extract some Wireless Parameter out of the driver
 * Use standard wrapper and add pretty error message if fail...
 */
#define IW_GET_EXT_ERR(skfd, ifname, request, wrq, rname) \
    do { \
    if(iw_get_ext(skfd, ifname, request, wrq) < 0) { \
        ERR_SET_EXT(rname, request); \
        fprintf(stderr, "    GET failed on device %-1.16s ; %s.\n", \
            ifname, strerror(errno)); \
        return(-6); \
    } } while(0)

#endif

class WifiConfigIwLib: public WifiConfig
{
  public:
#ifdef HAVE_LIBIW
    static int sock_iwconfig;
    static int sock_iwconfig_refs;
#endif

    WifiConfigIwLib(String dev_name);
    ~WifiConfigIwLib();

    int set_txpower(int txpower);
    int get_txpower();
    int get_max_txpower();

    int get_rates(Vector<MCS> &rates);

    int get_channel();
    int set_channel(int channel);

    void get_cca(int *cs_threshold, int *rx_threshold, int *cp_threshold);
    void set_cca(int cs_threshold, int rx_threshold, int cp_threshold);

    uint32_t set_backoff(uint32_t *_queue_info);
    uint32_t get_backoff(uint32_t **_queue_info);

  private:
#ifdef HAVE_LIBIW
    int get_info(struct wireless_info *info);
#endif

};

CLICK_ENDDECLS
#endif
