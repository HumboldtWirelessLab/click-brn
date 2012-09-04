/*
 * ShamirServer.hh
 *
 *  Created on: 04.09.2012
 *      Author: Dominik Oepen
 */

#define MAX_SHARESIZE 256 // Current max RSA modulus is 2048 Bit (256 Byte)

struct shamir_reply {
    unsigned int share_id;
    unsigned int share_len;
    unsigned char share[MAX_SHARESIZE];
};
