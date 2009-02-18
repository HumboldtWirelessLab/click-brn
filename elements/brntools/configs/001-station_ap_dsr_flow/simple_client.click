BRNAddressInfo(deviceaddress eth0:eth);
BRNAddressInfo(my_wlan eth0:eth);

wireless::BRN2Device(DEVICENAME "eth0", ETHERADDRESS deviceaddress, DEVICETYPE "WIRELESS");

id::BRN2NodeIdentity(wireless);

wlan_out_queue :: NotifierQueue(50);

elementclass AdHocOrInfraStructureClient {
    ETH $eth, SSID $ssid, CHANNEL $channel , WIFIENCAP $clientwifiencap, 
    WIRELESS_INFO $auth_info |

    auth_rates :: AvailableRates(DEFAULT 2 4 11 12 18 22);

    probe_req :: ProbeRequester(WIRELESS_INFO $auth_info, ETH $eth, RT auth_rates);
    auth_req :: OpenAuthRequester(ETH $eth, WIRELESS_INFO $auth_info);
    assoc_req :: BRNAssocRequester(ETH $eth, WIRELESS_INFO $auth_info, RT auth_rates, DEBUG 0);

    bs :: BRNBeaconScanner(RT auth_rates);
 
    isc :: InfrastructureClient(WIRELESS_INFO $auth_info, RT auth_rates, 
      BEACONSCANNER bs, PROBE_REQUESTER probe_req, AUTH_REQUESTER auth_req, 
      ASSOC_REQUESTER assoc_req, WIFIENCAP $clientwifiencap );

	  //all :: CompoundHandler("debug", "BRNAssocRequester InfrastructureClient", "2");
	  

    input[0]
    -> mgt_cl :: Classifier(0/00%f0, //Association Request
                            0/10%f0, //Association Response
                            0/20%f0, //Reassociation Request            
                            0/30%f0, //Reassociation Response
		        0/40%f0, //Probe Request 
                            0/50%f0, //Probe Response
                            0/80%f0, //Beacon
			    0/90%f0, //ATIM (Power Save)
                            0/a0%f0, //Deassociation
                            0/b0%f0, //Authentification
			    0/c0%f0  //De-Authentification
          );


    mgt_cl[0]  //Association Request
    -> Discard;    

    mgt_cl[1]  //Association Response
//    -> Print("Bekomme Association Response")
    -> assoc_req
//    -> Print("Sende Association Request")
    -> [0]output;

    mgt_cl[2]  //Reassociation Request
    -> Discard;

    mgt_cl[3]  //Reassociation Response
//  -> Print("Bekomme Reassociation Response")
    -> assoc_req;

    mgt_cl[4]  //Probe Request
    -> Discard;

    mgt_cl[5]  //Probe Response
//    -> PrintWifi("Bekomme Probe Response in den Beaconscanner")
    -> bs
    -> Discard;
    
    probe_req
//    -> Print("Send Probe",64)
    -> [0]output;

    mgt_cl[6]  //Beacon
//    -> PrintWifi("Bekomme Beacon: ")
//    -> st_hnd
    -> bs;

    mgt_cl[7]  //ATIM (Power Save)
    -> Discard;

    mgt_cl[8]  //Deassociation
//  -> Print("Bekomme deassoc",64)
    -> assoc_req;

    mgt_cl[9]  //Authentification
//    -> Print("bekomme AUTH")
    -> auth_req    
//    -> Print("Send AUTH ",128)
    -> [0]output;

    mgt_cl[10]  //De-Authentification
    -> Discard;

}

auth_info :: WirelessInfo(SSID brn2, BSSID 00:00:00:00:00:00 , CHANNEL 5);
infra_wifiencap ::  WifiEncap(0x01, WIRELESS_INFO auth_info);

infra_client :: AdHocOrInfraStructureClient( ETH my_wlan, SSID ##brn3##, CHANNEL 1, WIFIENCAP infra_wifiencap, WIRELESS_INFO auth_info );

rates :: AvailableRates(DEFAULT 2 4 11 12 18 22);

// ----------------------------------------------------------------------------
// Handling ath0-device
// ----------------------------------------------------------------------------
FromSimDevice(eth0,4096) //, PROMISC true
  -> SetTXRate(22)
  -> Strip(14)
//  -> Print("Receiver Client")
  -> FilterPhyErr()
//  -> Print("Client: FromDevice", 100)
  -> filter :: FilterTX();

filter[0]
  -> WifiDupeFilter()
  -> mgm_clf :: Classifier(0/00%0f, -); // management frames

mgm_clf[0] //handle mgmt frames
  -> infra_client
  -> SetTXRate(22) // ap beacons send at constant bitrate
  -> wlan_out_queue;

mgm_clf[1] //handle other frames (data)
//  -> Print("Client: Data")
  -> WifiDecap()
//  -> Print("Client: Ether Data",60)
  -> clf_bcast :: Classifier(0/ffffffffffff, -); // broadcast & unicast packets

clf_bcast[0]
  -> Discard();

//
// handle Unicast
//
clf_bcast[1]                                      //Unicast
  -> Print("Client: Unicast (in): ",256)           //simple packets (no brn) (only IP)
  -> Classifier(12/0800)                    // ip packet (don't handle IP-Packets in simulation -> Discard
  -> EtherDecap()
  -> CheckIPHeader()
  -> IPPrint("Client")
  -> Print("---------------- End: Got an IP-packet. Replay.")
  -> Discard();

//BRN intern and other Clients

//protoclf[0]                           //brn packets
//  -> Discard();

  wlan_out_queue
  -> AddEtherNsclick()
//  -> Print("ToSim")
  -> ToSimDevice(eth0);

filter[1]                                              //take a closer look at tx-annotated packets
  -> failures :: FilterFailures();

failures[0]
  -> Discard;

failures[1]
  -> WifiDupeFilter()
  -> Discard();

Idle
-> infra_wifiencap
-> Discard;
 
Script(
  wait 12,
  read infra_client/isc.wireless_info,
  read infra_client/isc.assoc
);
