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

typedef struct {													//             rx/tx?    // TODO:  beim empfangen auswerten, ob packet CRC ok ist
    unsigned portCHAR  status;									 	// State:   echo_ok?, echo_error?;  crc? , no_tx?, hw_rxtx_test?  ...
    unsigned portCHAR  count;										
    unsigned portCHAR  channel;                          							// channel frequency:      2400 MHz + rf_ch * a MHz       ( a=1 für 1 Mbps, 2 für 2 Mbps )
    unsigned portCHAR  rate;									 		// data rate value:      	  1 = 1 Mbps   ,		2 = 2 Mbps
    unsigned portCHAR  power;   					     				 	// power:		        	00 =  -18 dBm,		01 = -12 dBm		10 = -6 dBm		11 = 0 dBm
    unsigned portCHAR  openbeacon_dmac[ OPENBEACON_MACSIZE ];	 		// kann von 3-5 Byte variieren
    unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];		 	// kann von 3-5 Byte variieren
} __attribute__ ((packed)) Click2OBD_header;   // 15


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

portCHAR putDataToUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR blen);
portCHAR getDataFromUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR *blen );
unsigned int write_to_channel( portCHAR* out, portLONG len, portLONG device );
unsigned int read_to_channel( portCHAR* out, portLONG len, portLONG device );
		
void debug_msg(char* msg, unsigned portCHAR msg_len);
void debug_hex_msg(char* msg, unsigned portCHAR msg_len);

#define USBCHANNEL_BUFFER_SIZE     500
typedef struct {
	portCHAR *getDataFromUSBChannel_buffer;
	portLONG getDataFromUSBChannel_buffer_len;
	portCHAR *putDataToUSBChannel_buffer;
	portLONG putDataToUSBChannel_buffer_len;	
	portCHAR *tmp_buffer;
	portLONG tmp_buffer_len;
	portLONG putDataToUSBChannel_lastpacket;
} __attribute__ ((packed)) static_buffer_info;

extern static_buffer_info sbi_dev[];

#define STATUS_OK							0
#define STATUS_ERROR_NO_DATA				-1
#define STRUKTUR_BUFFER_MAX_LENGTH	       200

#ifdef __OPENBEACON_COMUNICATION_H__WITH_ENCODING	
	typedef struct {
		unsigned portCHAR buffer[ STRUKTUR_BUFFER_MAX_LENGTH ];
		unsigned portCHAR length; 
	} __attribute__ ((packed)) StrukturBuffer;
	
	/*
		Send and Recive Packets over USB(HOST)
	*/
	portCHAR putDataToUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR blen) {
		OBD2HW_Header *ph = (OBD2HW_Header *)sbi_dev[fd].putDataToUSBChannel_buffer;
		unsigned portCHAR tmp_buffer[150];
		unsigned int i, j;
		
		if( USBCHANNEL_BUFFER_SIZE-sbi_dev[fd].putDataToUSBChannel_buffer_len>blen) {
			memcpy(sbi_dev[fd].putDataToUSBChannel_buffer+sbi_dev[fd].putDataToUSBChannel_buffer_len, buffer, blen);
			sbi_dev[fd].putDataToUSBChannel_buffer_len += blen;
		} else { // TODO: other error?
			return STATUS_ERROR_NO_DATA;
		}

		if(sbi_dev[fd].putDataToUSBChannel_buffer_len>= (portLONG)(sizeof(OBD2HW_Header)+ph->length) ) { // can full send
			tmp_buffer[0] = sbi_dev[fd].putDataToUSBChannel_buffer[0];     //  is 0
			j=1;
			for(i=1; i<sizeof(OBD2HW_Header)+ph->length; i++) {
				// encoding for usb-channel
				if( sbi_dev[fd].putDataToUSBChannel_buffer[i]==0x00 || (sbi_dev[fd].putDataToUSBChannel_buffer[i]>0x00 && sbi_dev[fd].putDataToUSBChannel_buffer[i]<=0x1F) ) {
					tmp_buffer[j] = 0x01;
					j++;
					tmp_buffer[j] = sbi_dev[fd].putDataToUSBChannel_buffer[i]+0x20;
				} else {
					tmp_buffer[j] = sbi_dev[fd].putDataToUSBChannel_buffer[i];
				}				
				j++;
			}		
			tmp_buffer[j] = 0;  write_to_channel( (char*)tmp_buffer, j+1, fd );
			
			sbi_dev[fd].putDataToUSBChannel_buffer_len -= sizeof(OBD2HW_Header)+ph->length;
			if(sbi_dev[fd].putDataToUSBChannel_lastpacket<0) sbi_dev[fd].putDataToUSBChannel_lastpacket = 0;
			
			memmove( sbi_dev[fd].putDataToUSBChannel_buffer, sbi_dev[fd].putDataToUSBChannel_buffer+sizeof(OBD2HW_Header)+ph->length, sbi_dev[fd].putDataToUSBChannel_buffer_len );
		}		
		return STATUS_OK;
	}
	portCHAR getDataFromUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR *blen ) {
		portLONG prev_len;	
		OBD2HW_Header *ph;
		int i, j;
		int tmp_switch_break;
		
		while(1) {		
			if( sbi_dev[fd].getDataFromUSBChannel_buffer_len > (portLONG)sizeof(OBD2HW_Header) ) {
				ph = (OBD2HW_Header *) sbi_dev[fd].getDataFromUSBChannel_buffer;
				
				if( ph->start==0 &&  sbi_dev[fd].getDataFromUSBChannel_buffer_len >= (portLONG)(sizeof(OBD2HW_Header) + ph->length)  	// ) {// packet full recive
					&& *blen>= sizeof(OBD2HW_Header) + ph->length) { 
						
					for(i=0; i<(int)(sizeof(OBD2HW_Header)+ph->length); i++) {	
						buffer[i] = sbi_dev[fd].getDataFromUSBChannel_buffer[i];
						*blen = sizeof(OBD2HW_Header)+ph->length;
					}			
					
					sbi_dev[fd].getDataFromUSBChannel_buffer_len -= *blen;
					if(sbi_dev[fd].putDataToUSBChannel_lastpacket>0) sbi_dev[fd].putDataToUSBChannel_lastpacket -= sizeof(OBD2HW_Header)+ph->length;
					
					memmove( sbi_dev[fd].getDataFromUSBChannel_buffer, sbi_dev[fd].getDataFromUSBChannel_buffer+(*blen), sbi_dev[fd].getDataFromUSBChannel_buffer_len );
					return STATUS_OK;
				}
			}
			prev_len = sbi_dev[fd].getDataFromUSBChannel_buffer_len;
			if( USBCHANNEL_BUFFER_SIZE-sbi_dev[fd].getDataFromUSBChannel_buffer_len>0 ) {
				if(150-sbi_dev[fd].tmp_buffer_len>0) sbi_dev[fd].tmp_buffer_len += read_to_channel( (char*) (sbi_dev[fd].tmp_buffer+sbi_dev[fd].tmp_buffer_len),  150-sbi_dev[fd].tmp_buffer_len, fd);

				j=0;
				tmp_switch_break = 0;
				for(i=0; i<sbi_dev[fd].tmp_buffer_len && tmp_switch_break==0 && j+sbi_dev[fd].getDataFromUSBChannel_buffer_len<USBCHANNEL_BUFFER_SIZE; i++) {
					// decoding data from usb-channel					
					switch( sbi_dev[fd].tmp_buffer[i] ) {
						case 0x00:	if(i<(sbi_dev[fd].tmp_buffer_len-1) ) {
										if( sbi_dev[fd].tmp_buffer[i+1]!=0) {
											if( ((sbi_dev[fd].getDataFromUSBChannel_buffer_len-sbi_dev[fd].putDataToUSBChannel_lastpacket)+j )>0 ) { 								
												if( ((sbi_dev[fd].getDataFromUSBChannel_buffer_len-sbi_dev[fd].putDataToUSBChannel_lastpacket)+j )>=(portLONG)sizeof(OBD2HW_Header) ) {
													ph = (OBD2HW_Header *) (sbi_dev[fd].getDataFromUSBChannel_buffer+sbi_dev[fd].putDataToUSBChannel_lastpacket);
													sbi_dev[fd].getDataFromUSBChannel_buffer_len = sbi_dev[fd].putDataToUSBChannel_lastpacket+sizeof(OBD2HW_Header)+ph->length;
													j = 0;													
												} else {
													sbi_dev[fd].getDataFromUSBChannel_buffer_len = sbi_dev[fd].putDataToUSBChannel_lastpacket;
													j = 0;														
												}
											}

											sbi_dev[fd].getDataFromUSBChannel_buffer[sbi_dev[fd].getDataFromUSBChannel_buffer_len+j] = sbi_dev[fd].tmp_buffer[i];
											sbi_dev[fd].putDataToUSBChannel_lastpacket = sbi_dev[fd].getDataFromUSBChannel_buffer_len+j;
										} else {
											j--;
										}
									} else {
										tmp_switch_break=1;
										j--;
										i--;
									}									
									break;
						case 0x01:
									if(i<(sbi_dev[fd].tmp_buffer_len-1) ) {
										i++;
										sbi_dev[fd].getDataFromUSBChannel_buffer[sbi_dev[fd].getDataFromUSBChannel_buffer_len+j] = sbi_dev[fd].tmp_buffer[i]-0x20;
									} else {
										tmp_switch_break=1;
										j--;
										i--;
									}
									break;
						default:
									sbi_dev[fd].getDataFromUSBChannel_buffer[sbi_dev[fd].getDataFromUSBChannel_buffer_len+j] = sbi_dev[fd].tmp_buffer[i];
					}
					j++;
				}
				sbi_dev[fd].getDataFromUSBChannel_buffer_len += j;
				sbi_dev[fd].tmp_buffer_len -= i;
				memcpy(sbi_dev[fd].tmp_buffer, sbi_dev[fd].tmp_buffer+i, sbi_dev[fd].tmp_buffer_len);
			}

			if(prev_len == sbi_dev[fd].getDataFromUSBChannel_buffer_len) {
				return STATUS_ERROR_NO_DATA;
			}
		}		
		return STATUS_ERROR_NO_DATA;
	}
#endif

#endif/*__OPENBEACON_COMUNICATION_H__*/
