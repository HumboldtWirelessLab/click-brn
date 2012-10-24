#ifndef __OPENBEACON_COMUNICATION_H__
#define __OPENBEACON_COMUNICATION_H__

#define OPENBEACON_MACSIZE         		5
#define ENCODING_PARAMETER				0x20

#ifndef portCHAR
	#define portCHAR	char
#endif
#ifndef portLONG
	#define portLONG	int
#endif
#ifndef portTickType
	#define portTickType	unsigned long  // 4 Byte
#endif

/*
	Communication Protocol Header for communication between Openbeacon Daemon and Openbeacon HW.
*/
#define MONITOR_INPUT 			1	+ ENCODING_PARAMETER
#define TEST_DATA				2 	+ ENCODING_PARAMETER
#define TEST_DATA_ECHO			3 	+ ENCODING_PARAMETER
#define PACKET_DATA 			10 	+ ENCODING_PARAMETER
#define CONFIG_DATA 			11 	+ ENCODING_PARAMETER
#define DEBUG_PRINT 			12 	+ ENCODING_PARAMETER
#define MONITOR_PRINT 			13 	+ ENCODING_PARAMETER
#define MONITOR_HEX_PRINT   	14 	+ ENCODING_PARAMETER
#define DEBUG_HEX_PRINT			15 	+ ENCODING_PARAMETER
#define SPEZIAL_PRINT			16 	+ ENCODING_PARAMETER
#define STATUS_OPENBEACON_V1	17 	+ ENCODING_PARAMETER


typedef struct {
  unsigned portCHAR  start;    // begin of a packet
  unsigned portCHAR  length;   // length of the data
  unsigned portCHAR  type;	   // type of the data
  unsigned portCHAR  reserved; // ignore
} __attribute__((packed)) OBD2HW_Header;   // 4 Bytes

/*
	Comunication Protocol Header for comunication between Click and Openbeacon Deamon.
*/
#define STATUS_ECHO_OK			0x01				// set 1 for echo, if packet can send
#define STATUS_ECHO_ERROR		0x02				// set 1 for echo, if packet can't send
#define STATUS_CRC_CHECK		0x04				// hw set 1, if crc is ok
#define STATUS_NO_TX			0x08				// set 1, if packet no send
#define STATUS_hw_rxtx_test		0x10				// set 1, if hw must send [count ] packets
#define STATUS_full_test		0x20				// set 1, if packet send from HOST to HOST

typedef struct {		
																	//             rx/tx?
    unsigned portCHAR  status;									 	// State:   echo_ok?, echo_error?;  crc? , no_tx?, hw_rxtx_test?  ...
    unsigned portCHAR  length;
    unsigned portCHAR  count;										
    unsigned portCHAR  channel;                          							// channel frequency:      2400 MHz + rf_ch * a MHz       ( a=1 f�r 1 Mbps, 2 f�r 2 Mbps )
    unsigned portCHAR  rate;									 	// data rate value:      	  1 = 1 Mbps   ,		2 = 2 Mbps
    unsigned portCHAR  power;   					     		 	// power:		        	00 =  -18 dBm,		01 = -12 dBm		10 = -6 dBm		11 = 0 dBm
    unsigned portCHAR  openbeacon_dmac[ OPENBEACON_MACSIZE ];		// kann von 3-5 Byte variieren
    unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];		// kann von 3-5 Byte variieren
} __attribute__ ((packed)) Click2OBD_header;   // 16

typedef struct {
	unsigned char TxPowerLevel;
	unsigned char TxRate;
	unsigned char TxChannel;
	unsigned char NetID[5];

	/* Statistik  */
	unsigned char send_count[4];
	unsigned char send_fail_count[4];
	unsigned char recv_count[4];
	unsigned char recv_fail_count[4];
} StatusOpenbeacon_V1;


/*
	the struct for HW_RXTX_TEST
*/
typedef struct {
	unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];
	unsigned char prot_type[2];
	unsigned char test_type;          // 0 - HW <--> HW
									  // ....
	unsigned char drate;			  // Datenrate für den Test
} __attribute__ ((packed)) HW_rxtx_Test;


#endif/*__OPENBEACON_COMUNICATION_H__*/
