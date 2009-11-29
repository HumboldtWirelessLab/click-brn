#ifndef LEMPEL_ZIV_WELCH_HH
#define LEMPEL_ZIV_WELCH_HH

#include <click/vector.hh>

CLICK_DECLS

#define MAGIC_1     (0x1f)    /* from GNU/Linux version */
#define MAGIC_2     (0x9D)    /* from GNU/Linux version */
#define FIRST       (257)     /* from GNU/Linux version */
#define BIT_MASK    (0x1f)    /* from GNU/Linux version */
#define BLOCK_MODE  (0x80)    /* from GNU/Linux version */

#define BITS 12                   /* Setting the number of bits to 12, 13*/
#define HASHING_SHIFT (BITS-8)    /* or 14 affects several constants.    */
#define MAX_VALUE (1 << BITS) - 1 /* Note that MS-DOS machines need to   */
#define MAX_CODE MAX_VALUE - 1    /* compile their code in large model if*/
             /* 14 bits are selected.               */
#if BITS == 14
#define TABLE_SIZE 18041        /* The string table size needs to be a */
#endif                            /* prime number that is somewhat larger*/
#if BITS == 13                    /* than 2**BITS.                       */
#define TABLE_SIZE 9029
#endif
#if BITS <= 12
#define TABLE_SIZE 5021
#endif

#define LZW_DECODE_STACK_SIZE 8000
#define LZW_DECODE_ERROR -1
#define LZW_ENCODE_ERROR -1


class LZW
{
 public:

   // The encoder will test all max bitsizes from maxBits1 to maxBits2
    // if maxBits1 < maxBits2. In order to specify only one max bitsize
    // leave maxBits2 to 0.
  LZW();
  ~LZW();

  int encode(unsigned char *input, int inputlen, unsigned char *encoded, int max_encodedlen);
  int decode(unsigned char *encoded, int encodedlen, unsigned char *decoded, int max_decodedlen);

 private:

  void reset_tables();
  int find_match(unsigned int hash_prefix, unsigned int hash_character);
  unsigned char *decode_string(unsigned char *buffer,unsigned int code);
  unsigned int input_code(unsigned char *input, int *pos, int inputlen);
  void output_code(unsigned char *output, int *pos, unsigned int code, int max_outputlen);

  /******************************************************************************************/
  /*****************************  Unused GNU Code *******************************************/
  /******************************************************************************************/
  unsigned int mask(const int n_bits);
  unsigned int input_code_gnu(unsigned char *input, int *pos, int inputlen, const int n_bits);
  void output_code_gnu(unsigned char *output, int *pos, unsigned int code, const int n_bits);

  int *code_value;                  /* This is the code value array        */
  unsigned int *prefix_code;        /* This array holds the prefix codes   */
  unsigned char *append_character;  /* This array holds the appended chars */
  unsigned char *decode_stack;      /* This array holds the decoded string */

  unsigned int b_mask;
  int n_bits_prev;
  int input_bit_count;
  unsigned long input_bit_buffer;
  int output_bit_count;
  unsigned long output_bit_buffer;

  int _debug;
};

CLICK_ENDDECLS
#endif
