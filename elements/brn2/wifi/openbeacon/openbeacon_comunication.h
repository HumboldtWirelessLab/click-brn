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

#ifndef portCHAR
	#define portCHAR	char
#endif
#ifndef portLONG
	#define portLONG	int
#endif
#ifndef portTickType 
	#define portTickType	unsigned long
#endif

typedef struct {
  unsigned portCHAR  length;
  unsigned portCHAR  type;
  unsigned portCHAR  reserved;
} __attribute__((packed)) OBD2HW_Header;

/*
	Comunication Protocol Header for comunication between Click and Openbeacon Deamon.
*/
#define STATUS_ECHO_OK		0x01				// set 1 for echo, if packet can send 
#define STATUS_ECHO_ERROR		0x02				// set 1 for echo, if packet can't send
#define STATUS_CRC_CHECK		0x04				// hw set 1, if crc is ok
#define STATUS_NO_TX			0x08				// set 1, if packet no send
#define STATUS_hw_rxtx_test		0x10				// set 1, if hw must send [count ] packets

typedef struct {													//             rx/tx?    // TODO:  beim empfangen auswerten, ob packet CRC ok ist
    unsigned portCHAR  status;									 	// State:   echo_ok?, echo_error?;  crc? , no_tx?, hw_rxtx_test?  ...
    unsigned portCHAR  count;										
    unsigned portCHAR  channel;                          							// channel frequency:      2400 MHz + rf_ch * a MHz       ( a=1 für 1 Mbps, 2 für 2 Mbps )
    unsigned portCHAR  rate;									 		// data rate value:      	  1 = 1 Mbps   ,		2 = 2 Mbps
    unsigned portCHAR  power;   					     				 	// power:		        	00 =  -18 dBm,		01 = -12 dBm		10 = -6 dBm		11 = 0 dBm
    unsigned portCHAR  openbeacon_dmac[ OPENBEACON_MACSIZE ];	 		// kann von 3-5 Byte variieren
    unsigned portCHAR  openbeacon_smac[ OPENBEACON_MACSIZE ];		 	// kann von 3-5 Byte variieren
} __attribute__ ((packed)) Click2OBD_header;


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
} __attribute__ ((packed)) HW_rxtx_Test;

portCHAR putDataToUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR blen);
portCHAR getDataFromUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR *blen );
unsigned int write_to_channel( portCHAR* out, unsigned portCHAR len, portLONG device );
unsigned int read_to_channel( portCHAR* out, unsigned portCHAR len, portLONG device );
		
void debug_msg(char* msg, unsigned portCHAR msg_len);

#define USBCHANNEL_BUFFER_SIZE     500
extern portCHAR getDataFromUSBChannel_buffer[];
extern portCHAR putDataToUSBChannel_buffer[];

	#define STATUS_OK							0
	#define STATUS_ERROR_NO_DATA				-1
	#define STRUKTUR_BUFFER_MAX_LENGTH	       200

	/*
		De-/Encoding for transmit data over 
                
		0x00   <-->   0x01 0x30
		0x01   <-->   0x01 0x31
                0x02   <-->   0x01 0x32   
			...
		0x1F   <->    0x01 0x4F

		j=0;
		k=0;
		for(i=0; i<b->length && k<max; i++) {
			if(encoding) {
				if( b->buffer[i] >= 0x00 && b->buffer[i] <= 0x0F ) {  // bytes 0x00 - 0x0F transform to 0x0102 - 0x0111
					b->buffer[b->block + j] = 0x01;  j++;
					b->buffer[b->block + j] = b->buffer[i] + 2;  j++;
				} else {
					b->buffer[b->block + j] = b->buffer[i];  j++;
				}
			} else {
				if( b->buffer[i]==0x01 ) {
					i++;
					b->buffer[b->block + j] = b->buffer[i] - 2;   // TODO: bugs ??
				} else {
					b->buffer[b->block + j] = b->buffer[i];
				}
				j++;
			}
			k++;
		}
	*/
#ifdef __OPENBEACON_COMUNICATION_H__WITH_ENCODING	
	typedef struct {
		unsigned portCHAR buffer[ STRUKTUR_BUFFER_MAX_LENGTH ];
		unsigned portCHAR length; 
	} __attribute__ ((packed)) StrukturBuffer;
	
	/*
		Send and Recive Packets over USB(HOST)
	*/
	portCHAR putDataToUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR blen) {
		
		static unsigned portLONG len=0;
		OBD2HW_Header *ph = (OBD2HW_Header *)putDataToUSBChannel_buffer;

		if( USBCHANNEL_BUFFER_SIZE-len>blen) {
			memcpy(putDataToUSBChannel_buffer+len, buffer, blen);
			len += blen;
		} else { // TODO: other error?
			return STATUS_ERROR_NO_DATA;
		}

		if(len>= sizeof(OBD2HW_Header)+ph->length) { // can full send
			// TODO: encoding for usb-channel
			
			write_to_channel( (char*)putDataToUSBChannel_buffer, sizeof(OBD2HW_Header)+ph->length, fd );
			len -= sizeof(OBD2HW_Header)+ph->length;
			memmove( putDataToUSBChannel_buffer, putDataToUSBChannel_buffer+sizeof(OBD2HW_Header)+ph->length, len );
		}		

// write_to_channel( (char*)buffer, blen, fd );
		
		return STATUS_OK;
	}
	portCHAR getDataFromUSBChannel(portLONG fd,  unsigned portCHAR* buffer,  unsigned portCHAR *blen ) {
		static unsigned portLONG len=0, prev_len;
			
		OBD2HW_Header *ph;
		unsigned int i;
		
		while(1) { 	
			if( len > sizeof(OBD2HW_Header) ) {
				ph = (OBD2HW_Header *) getDataFromUSBChannel_buffer;
				
				if(len >= sizeof(OBD2HW_Header) + ph->length  	// ) {// packet full recive
					&& *blen>= sizeof(OBD2HW_Header) + ph->length) { 
						
					// TODO: buffer overflow
					for(i=0; i<sizeof(OBD2HW_Header)+ph->length; i++) {
						// TODO: decoding from usb-channel
			
						buffer[i] = getDataFromUSBChannel_buffer[i];
						*blen = sizeof(OBD2HW_Header)+ph->length;
					}			
					
					len -= *blen;
					memmove( getDataFromUSBChannel_buffer, getDataFromUSBChannel_buffer+(*blen), len );
				
					return STATUS_OK;
				}
			}
			prev_len = len;
			if( USBCHANNEL_BUFFER_SIZE-len>0 ) {
				len += read_to_channel( (char*)(getDataFromUSBChannel_buffer+len), USBCHANNEL_BUFFER_SIZE-len, fd );
			}

			if(prev_len == len) return STATUS_ERROR_NO_DATA;
		}		
		return STATUS_ERROR_NO_DATA;
	}
#endif

#endif/*__OPENBEACON_COMUNICATION_H__*/
