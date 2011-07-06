// TODO: make function putData* and getData* THREAD-SAVE

#ifndef __OPENBEACON_COMUNICATION_H__
#define __OPENBEACON_COMUNICATION_H__

#define OPENBEACON_MACSIZE                    5

/*
	Comunication Protocol Header for comunication between Openbeacon Deamon and Openbeacon HW.
*/
#define UNKNOWN_DATA 		0
#define PACKET_DATA 		1
#define CONFIG_DATA 		2
#define DEBUG_PRINT 		3
#define MONITOR_PRINT 		4
#define MONITOR_INPUT 		5
#define MONITOR_HEX_PRINT   6
#define DEBUG_HEX_PRINT	7

#ifndef portCHAR
	#define portCHAR	char
#endif
#ifndef portLONG
	#define portLONG	int
#endif
#ifndef portTickType 
	#define portTickType	unsigned short
#endif

typedef struct {
  unsigned portCHAR  start;
  unsigned portCHAR  length;
  unsigned portCHAR  type;
  unsigned portCHAR  reserved;
} __attribute__((packed)) OBD2HW_Header;   // 4 Bytes

/*
	Comunication Protocol Header for comunication between Click and Openbeacon Deamon.
*/
#define STATUS_ECHO_OK		0x01				// set 1 for echo, if packet can send 
#define STATUS_ECHO_ERROR		0x02				// set 1 for echo, if packet can't send
#define STATUS_CRC_CHECK		0x04				// hw set 1, if crc is ok
#define STATUS_NO_TX			0x08				// set 1, if packet no send
#define STATUS_hw_rxtx_test		0x10				// set 1, if hw must send [count ] packets
#define STATUS_full_test			0x20				// set 1, if packet send from HOST to HOST

typedef struct {													             
    unsigned portCHAR  status;									 	// State:   echo_ok?, echo_error?;  crc? , no_tx?, hw_rxtx_test?  ...
    unsigned portCHAR  count;										
    unsigned portCHAR  channel;                          							// channel frequency:      2400 MHz + rf_ch * a MHz       ( a=1 für 1 Mbps, 2 für 2 Mbps )
    unsigned portCHAR  rate;									 		// data rate value:      	  1 = 1 Mbps   ,		2 = 2 Mbps
    unsigned portCHAR  power;   					     				 	// power:		        	00 =  -18 dBm,		01 = -12 dBm		10 = -6 dBm		11 = 0 dBm
    unsigned portCHAR  openbeacon_dmac[ OPENBEACON_MACSIZE ];	 		// kann von 3-5 Byte variieren
    unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];		 	// kann von 3-5 Byte variieren
    unsigned portCHAR  length;										 
} __attribute__ ((packed)) Click2OBD_header;   // 16


/*
	the struct for HW_RXTX_TEST
*/
typedef struct {
	unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];		 	// kann von 3-5 Byte variieren
	unsigned char prot_type[2];										// Protokolltype: 0606  
	unsigned portCHAR type;
	unsigned portCHAR count;
	unsigned portCHAR number;
	portTickType timestamp_send;	
	portTickType timestamp_recive;
	// extensions
	// Testbegin, Testend
	unsigned portLONG test_begin;
	unsigned portLONG test_end;
	unsigned portLONG test_time;
} __attribute__ ((packed)) HW_rxtx_Test;   // 5 + 5 + 5*4 = 20 + 10 = 30


#endif/*__OPENBEACON_COMUNICATION_H__*/
