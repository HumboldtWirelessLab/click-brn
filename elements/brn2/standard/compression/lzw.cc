#include <click/config.h>
#include "lzw.hh"

CLICK_DECLS

LZW::LZW()
{
  //TODO: Handle malloc-error
  code_value= new int32_t[TABLE_SIZE];
  prefix_code=new uint32_t[TABLE_SIZE];
  append_character=new unsigned char[TABLE_SIZE];
  decode_stack=new unsigned char[LZW_DECODE_STACK_SIZE];

  if (code_value==NULL || prefix_code==NULL || append_character==NULL || decode_stack==NULL)
    click_chatter("Fatal error allocating table space!\n");
  else
    reset_tables();
}

LZW::~LZW()
{
  delete[] code_value;
  delete[] prefix_code;
  delete[] append_character;
  delete[] decode_stack;
}

void
LZW::reset_tables()
{
  memset(code_value,0, TABLE_SIZE*sizeof(int32_t));
  memset(prefix_code,0,TABLE_SIZE*sizeof(uint32_t));
  memset(append_character,0,TABLE_SIZE*sizeof(unsigned char));

  b_mask = 0;
  n_bits_prev = 0;
  input_bit_count = 0;
  input_bit_buffer = 0L;
  output_bit_count = 0;
  output_bit_buffer = 0L;
}

/*
** This routine simply decodes a string from the string table, storing
** it in a buffer.  The buffer can then be output in reverse order by
** the expansion program.
*/

unsigned char *
LZW::decode_string(unsigned char *buffer, uint32_t code)
{
  int32_t i = 0;

  while (code > 255)
  {
    if ( code >= TABLE_SIZE ) return NULL;                     //HINT; Just to handle corrupt packets
    *buffer++ = append_character[code];
    code=prefix_code[code];
    if (i++ >= MAX_CODE) {
      click_chatter("Fatal error during code expansion.\n");
      return NULL;
    }
  }

  *buffer=code;

  return(buffer);
}

/*
** This is the hashing routine.  It tries to find a match for the prefix+char
** string in the string table.  If it finds it, the index is returned.  If
** the string is not found, the first available index in the string table is
** returned instead.
*/

int32_t
LZW::find_match(uint32_t hash_prefix, uint32_t hash_character)
{
  int32_t index = 0;
  int32_t offset = 0;

  index = (hash_character << HASHING_SHIFT) ^ hash_prefix;
  if (index == 0)
    offset = 1;
  else
    offset = TABLE_SIZE - index;
  while (1)
  {
    if (code_value[index] == -1)
      return(index);
    if (prefix_code[index] == hash_prefix &&
        append_character[index] == hash_character)
      return(index);
    index -= offset;
    if (index < 0)
      index += TABLE_SIZE;
  }
}

uint32_t
LZW::input_code(unsigned char *input, int32_t *pos, int32_t inputlen)
{
  uint32_t c;
  uint32_t return_value;

  while (input_bit_count <= 24)
  {
    if ( *pos >= inputlen ) return MAX_VALUE;
    c = input[*pos];
    *pos = *pos + 1;
    input_bit_buffer |= c << (24-input_bit_count);
    input_bit_count += 8;
  }

  return_value=input_bit_buffer >> (32-BITS);
  input_bit_buffer <<= BITS;
  input_bit_count -= BITS;

  return return_value;
}

void
LZW::output_code(unsigned char *output, int32_t *pos, uint32_t code, int32_t max_outputlen)
{
  unsigned char outc;

  output_bit_buffer |= code << (32-BITS-output_bit_count);
  output_bit_count += BITS;

  while (output_bit_count >= 8)
  {
    outc = output_bit_buffer >> 24;
//  click_chatter("Out: %d",outc);
    output[*pos] = outc;
    *pos = *pos + 1;
    output_bit_buffer <<= 8;
    output_bit_count -= 8;

    if ( *pos >= max_outputlen ) {
      *pos = -1;
      return;
    }
  }
}

/*
** This is the compression routine.  The code should be a fairly close
** match to the algorithm accompanying the article.
**
*/
int
LZW::encode(unsigned char *input, int inputlen, unsigned char *output, int max_outputlen)
{
  uint32_t next_code;
  uint32_t character;
  uint32_t string_code;
  uint32_t index = 0;
  int32_t inputpos;
  int32_t outputpos;
  int32_t i;

  reset_tables();

  inputpos = 0;
  outputpos = 0;

  next_code=256;                /* Next code is the next available string code*/

  for (i=0; i < TABLE_SIZE; i++)  /* Clear out the string table before starting */
    code_value[i]=-1;

  i=0;
  string_code=input[inputpos]; inputpos++;/* Get the first code                         */

  /*
  ** This is the main loop where it all happens.  This loop runs util all of
  ** the input has been exhausted.  Note that it stops adding codes to the
  ** table after all of the possible codes have been defined.
*/
  while (inputpos < inputlen)
  {
//  click_chatter("Inputpos: %d",inputpos);
    character = input[inputpos]; inputpos++;
    index = find_match(string_code,character);/* See if the string is in */
    if (code_value[index] != -1)            /* the table.  If it is,   */
      string_code=code_value[index];        /* get the code value.  If */
    else                                    /* the string is not in the*/
    {                                       /* table, try to add it.   */
      if (next_code <= MAX_CODE)
      {
        code_value[index]=next_code++;
        prefix_code[index]=string_code;
        append_character[index]=character;
      }

      output_code(output, &outputpos, string_code, max_outputlen);  /* When a string is found  */
      if ( outputpos == LZW_ENCODE_ERROR ) return LZW_ENCODE_ERROR;

      string_code=character;            /* that is not in the table*/
    }                                   /* I output the last string*/
  }                                     /* after adding the new one*/

  /*
  ** End of the main loop.
  */
  output_code(output, &outputpos, string_code, max_outputlen); /* Output the last code               */
  if ( outputpos == LZW_ENCODE_ERROR ) return LZW_ENCODE_ERROR;
  output_code(output, &outputpos, MAX_VALUE, max_outputlen);   /* Output the last code               */
  if ( outputpos == LZW_ENCODE_ERROR ) return LZW_ENCODE_ERROR;
  output_code(output, &outputpos, 0, max_outputlen);           /* This code flushes the output buffer*/
  if ( outputpos == LZW_ENCODE_ERROR ) return LZW_ENCODE_ERROR;

  return outputpos;
}

/*
**  This is the expansion routine.  It takes an LZW format file, and expands
**  it to an output file.  The code here should be a fairly close match to
**  the algorithm in the accompanying article.
*/
int
LZW::decode(unsigned char *input, int inputlen, unsigned char *output, int max_outputlen)
{
  uint32_t next_code;
  uint32_t new_code;
  uint32_t old_code;
  int32_t character;
  int32_t counter;
  unsigned char *string;
  int32_t inputpos;
  int32_t outputpos;

  if ( max_outputlen == 0 ) return LZW_DECODE_ERROR;

  reset_tables();

  inputpos=0;
  outputpos=0;             /* position inside outputbuffer */

  new_code=256;            /* Init value. not need but better it avoids valgrind errors */
  next_code=256;           /* This is the next available code to define */
  counter=0;               /* Counter is used as a pacifier.            */

  old_code=input_code(input, &inputpos, inputlen);  /* Read in the first code, initialize the */
  character=old_code;                               /* character variable, and send the first */
  output[outputpos] = old_code; outputpos++;

  if ( outputpos >= max_outputlen ) return LZW_DECODE_ERROR;

  /*
  **  This is the main expansion loop.  It reads in characters from the LZW file
  **  until it sees the special code used to inidicate the end of the data.
  */
  while ( ( new_code = input_code(input, &inputpos, inputlen) ) != MAX_VALUE)
  {
    /*
    ** This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING
    ** case which generates an undefined code.  It handles it by decoding
    ** the last code, and adding a single character to the end of the decode string.
    */
    if (new_code>=next_code)
    {
      *decode_stack=character;
      string=decode_string(decode_stack + 1, old_code);
      if ( string == NULL ) return LZW_DECODE_ERROR;
    }
    /*
    ** Otherwise we do a straight decode of the new code.
    */
    else {
      string=decode_string(decode_stack,new_code);
      if ( string == NULL ) return LZW_DECODE_ERROR;
    }

    /*
    ** Now we output the decoded string in reverse order.
    */
    character=*string;

    if ( (outputpos + string - decode_stack) >= max_outputlen ) return LZW_DECODE_ERROR;

    while (string >= decode_stack) {
      output[outputpos] = *string--;
      outputpos++;
    }
    /*
    ** Finally, if possible, add a new code to the string table.
    */
    if (next_code <= MAX_CODE)
    {
      prefix_code[next_code]=old_code;
      append_character[next_code]=character;
      next_code++;
    }
    old_code=new_code;
  }

  return outputpos;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(LZW)
