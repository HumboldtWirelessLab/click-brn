elementclass AccessPoint { DEVICE $device, ETHERADDRESS $etheraddress, SSID $ssid, CHANNEL $channel,
                           BEACON_INTERVAL $beacon_interval, LT $link_table, RATES $rates |

	assoclist :: BRN2AssocList(LINKTABLE $link_table);
    winfo :: WirelessInfo(SSID $ssid, BSSID $etheraddress, CHANNEL $channel, INTERVAL $beacon_interval);
    bs :: BeaconScanner(RT rates);

    input[0]
    -> mgt_cl :: Classifier( 0/00%f0, //assoc req
                             0/10%f0, //assoc resp
                             0/40%f0, //probe req
                             0/50%f0, //probe resp
                             0/80%f0, //beacon
                             0/a0%f0, //assoc
                             0/b0%f0, //disassoc
                                -  );

    mgt_cl[0]
//  -> Print("assocReq")
    -> BRN2AssocResponder(DEBUG 0, DEVICE $device, WIRELESS_INFO winfo,
                          RT rates, ASSOCLIST assoclist, RESPONSE_DELAY 0 )
    -> [0]output;

    mgt_cl[1]
//  -> Print("assocResp")
    -> Discard;

    mgt_cl[2]
//  -> Print("probereq")
    -> BRN2BeaconSource( WIRELESS_INFO winfo, RT rates,ACTIVE 1)
//  -> Print("proberesp")
    -> [0]output;

    mgt_cl[3]
//  -> Print("proberesp")
    -> Discard;

    mgt_cl[4]
//  -> Print("beacon")
    -> bs                    //BeaconScanner
    -> Discard; 

    mgt_cl[5]
//  -> Print("Dissas")
    -> Discard;

    mgt_cl[6]
//  -> Print("authReq")
    -> OpenAuthResponder(WIRELESS_INFO winfo)
    -> [0]output;
    
    mgt_cl[7]
//  -> Print("Unknow Managmentframe",10)
    -> Discard;
}

// input[0] - ethernet (802.3) frames from external nodes (no BRN protocol)
// input[1] - BRN DSR packets from internal nodes
// input[2] - failed transmission of a BRN DSR packet (broken link) from ds
// [0]output - ethernet (802.3) frames to external nodes/clients or me (no BRN protocol)
// [1]output - BRN DSR packets to internal nodes (BRN DSR protocol)

elementclass DSR {$ID, $LT, $RC |

  brn_encap :: BRN2Encap;
  dsr_decap :: BRN2DSRDecap(NODEIDENTITY $ID, LINKTABLE $LT);
  dsr_encap :: BRN2DSREncap(NODEIDENTITY $ID, LINKTABLE $LT);
  
  querier :: BRN2RouteQuerier(NODEIDENTITY $ID, LINKTABLE $LT, DSRENCAP dsr_encap, BRNENCAP brn_encap, DSRDECAP dsr_decap, DEBUG 0);

  req_forwarder :: BRN2RequestForwarder(NODEIDENTITY $ID, LINKTABLE $LT, DSRDECAP dsr_decap, DSRENCAP dsr_encap, BRNENCAP brn_encap, ROUTEQUERIER querier, MINMETRIC 15000, DEBUG 0);
  rep_forwarder :: BRN2ReplyForwarder(NODEIDENTITY $ID, LINKTABLE $LT, DSRDECAP dsr_decap, ROUTEQUERIER querier, DSRENCAP dsr_encap);
  src_forwarder :: BRN2SrcForwarder(NODEIDENTITY $ID, LINKTABLE $LT, DSRENCAP dsr_encap, DSRDECAP dsr_decap);
  err_forwarder :: BRN2ErrorForwarder(NODEIDENTITY $ID, LINKTABLE $LT, DSRENCAP dsr_encap, DSRDECAP dsr_decap, ROUTEQUERIER querier, BRNENCAP brn_encap);

  input[0]
//  -> Print("RouteQuery")
  -> querier[0]
//  -> Print("DSR: querie")
  -> [1]output;                                             // rreq packets (broadcast)
  
  querier[1] 
//  -> Print("DSR: src_forwarder")
  -> [0]src_forwarder;                                      // src routed packets (unicast)

  src_forwarder[0]
//  -> Print("Forward")
  -> [1]output;

  src_forwarder[1]
//  -> Print("Final dest")
  -> [0]output;

  src_forwarder[2]
//  -> Print("Error")
  -> tee_to_err_fwd :: Tee()
  -> Discard;                                                  //is for BRNiapp

  tee_to_err_fwd[1]
  -> [0]err_forwarder;

  // ------------------
  // internal packets
  // ------------------
  input[1]
  -> dsrclf :: Classifier( 6/01, //DSR_RREQ
                           6/02, //DSR_RREP
                           6/03, //DSR_RERR
                           6/04, //DSR_SRC
                         );

  dsrclf[0]
//  -> Print("Req_fwd_in")
  -> req_forwarder[0]
//  -> Print("Req_fwd_out")
  -> [1]output;

  req_forwarder[1]
//  -> Print("Target! now send reply")
  -> [0]rep_forwarder
  -> [1]output;

  dsrclf[1] 
//  -> Print("Route Reply")
  -> [1]rep_forwarder;

  dsrclf[2]
  -> [1]err_forwarder
  -> [1]output;

  dsrclf[3]
  -> [1]src_forwarder;

  // ------------------
  // undeliverable packets
  // ------------------
  input[2]
  -> [0]err_forwarder;
}


//output:
//  0: To me and BRN
//  1: Broadcast and BRN
//  2: Foreign and BRN
//  3: To me and NO BRN
//  4: Foreign and NO BRN
//  5: BROADCAST and NO BRN
//  6: Feedback BRN
//  7: Feedback Other
//
//input::
//  0: brn
//  1: client

elementclass WIFIDEV { DEVNAME $devname, DEVICE $device, ETHERADDRESS $etheraddress, SSID $ssid, CHANNEL $channel, LT $lt |

  nblist::BRN2NBList();  //stores all neighbors (known (friend) and unknown (foreign))
  nbdetect::NeighborDetect(NBLIST nblist, DEVICE $device);
  rates::AvailableRates(DEFAULT 2 4 11 12 18 22 24 36 48 72 96 108);
  proberates::AvailableRates(DEFAULT 2 22);
  etx_metric :: BRN2ETXMetric($lt);
  
  link_stat :: BRN2LinkStat(ETHTYPE          0x0a04,
                            DEVICE          $device,
                            PERIOD             3000,
                            TAU               30000,
                            ETX          etx_metric,
//                          PROBES  "2 250 22 1000",
                            PROBES  "2 250",
                            RT           proberates);
                            
   
  ap::AccessPoint(DEVICE $device, ETHERADDRESS $etheraddress, SSID "brn2",
                 CHANNEL $channel, BEACON_INTERVAL 100, LT $lt, RATES rates);

  toStation::BRN2ToStations(ASSOCLIST ap/assoclist);
  toMe::BRN2ToThisNode(NODEIDENTITY id);
  brnToMe::BRN2ToThisNode(NODEIDENTITY id);
  
  FromSimDevice($devname, SNAPLEN 1500)
  -> Strip(14)                              //-> ATH2Decap(ATHDECAP true)
//  -> Print("FromDev")
  -> wififrame_clf :: Classifier( 0/00%0f,  // management frames
                                      - ); 

  wififrame_clf[0]
    -> ap
    -> wifioutq::NotifierQueue(50)
    -> AddEtherNsclick()
    //-> Print("To Device")
    -> toDevice::ToSimDevice($devname);

  wififrame_clf[1]
    -> WifiDecap()
    -> nbdetect
//    -> Print("Data")
    -> brn_ether_clf :: Classifier( 12/8086, - )
    -> lp_clf :: Classifier( 14/06, - )
    -> EtherDecap()
    -> link_stat
    -> EtherEncap(0x8086, deviceaddress, ff:ff:ff:ff:ff:ff)
    -> brnwifi::WifiEncap(0x00, 0:0:0:0:0:0)
    -> wifioutq;
 
  brn_ether_clf[1]                                      //no brn
  -> toStation;

  toStation[0]
  -> Print("For a Station")
  -> clientwifi::WifiEncap(0x02, WIRELESS_INFO ap/winfo)
  -> wifioutq;

  toStation[1]
  -> Print("Broadcast")
  -> [4]output;

  toStation[2]
  -> toMe;
  
  toMe[1]         //broadcast
  -> [5]output;

  toMe[2]         //Foreign
  -> [4]output;
                            
  toMe[0]
  -> [3]output;   //to me but no brn            

  lp_clf[1]       //brn, but no lp
  -> Print("Data, no LP")
  -> brnToMe;
  
  brnToMe[0] -> Print("wifi0") -> [0]output;
  brnToMe[1] -> Print("wifi1") -> [1]output;
  brnToMe[2] -> Print("wifi2") -> [2]output;

  input[0] -> brnwifi;
  input[1] -> clientwifi;
  
} 

BRNAddressInfo(deviceaddress eth0:eth);
wireless::BRN2Device(DEVICENAME "eth0", ETHERADDRESS deviceaddress, DEVICETYPE "WIRELESS");

id::BRN2NodeIdentity(wireless);

rc::BrnRouteCache(DEBUG 0, ACTIVE false, DROP /* 1/20 = 5% */ 0, SLICE /* 100ms */ 0, TTL /* 4*100ms */4);
lt::Brn2LinkTable(NODEIDENTITIY id, ROUTECACHE rc, STALE 500,  SIMULATE false, CONSTMETRIC 1, MIN_LINK_METRIC_IN_ROUTE 15000);

device_wifi::WIFIDEV(DEVNAME eth0, DEVICE wireless, ETHERADDRESS deviceaddress, SSID "brn", CHANNEL 5, LT lt);

dsr::DSR(id,lt,rc);

device_wifi
-> EtherDecap()
-> brn_clf::Classifier(         0/03,  //BrnDSR
                                  -  );//other
                                    
brn_clf[0] -> /*Print("DSR-Packet") -> */ [1]dsr;
brn_clf[1] -> Discard;

device_wifi[1] -> /*Print("BRN-In") -> */ EtherDecap() -> brn_clf;
device_wifi[2] -> Discard;
device_wifi[3] -> Discard;
device_wifi[4] -> [0]dsr;
device_wifi[5] -> Discard;

Idle -> [2]dsr;

dsr[0] -> toMeAfterDsr::BRN2ToThisNode(NODEIDENTITY id);
dsr[1] -> BRN2EtherEncap() /*-> Print("DSR-OUT")*/ -> [0]device_wifi;

toMeAfterDsr[0] -> Print("DSR-out: For ME") -> Discard; 
toMeAfterDsr[1] -> Print("DSR-out: Broadcast") -> Discard;
toMeAfterDsr[2] -> Print("DSR-out: Foreign/Client") -> [1]device_wifi;

Script(
  wait 10,
//  read id.devinfo,
//  read nblist.neighbor,
  read lt.links,
  read lt.hosts,
  wait 15,
  read lt.links
);
