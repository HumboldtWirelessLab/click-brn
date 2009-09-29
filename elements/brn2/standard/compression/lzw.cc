#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <click/config.h>
#include "lzw.hh"

CLICK_DECLS

LZW::LZW()
{
  //TODO: Handle malloc-error
  code_value=(int*)malloc(TABLE_SIZE*sizeof(int));
  prefix_code=(unsigned int *)malloc(TABLE_SIZE*sizeof(unsigned int));
  append_character=(unsigned char *)malloc(TABLE_SIZE*sizeof(unsigned char));

  if (code_value==NULL || prefix_code==NULL || append_character==NULL)
    click_chatter("Fatal error allocating table space!\n");
  else
    reset_tables();
}

LZW::~LZW()
{
  free(code_value);
  free(prefix_code);
  free(append_character);
}

void
LZW::reset_tables()
{
  memset(code_value,0, TABLE_SIZE*sizeof(int));
  memset(prefix_code,0,TABLE_SIZE*sizeof(unsigned int));
  memset(append_character,0,TABLE_SIZE*sizeof(unsigned char));

  b_mask = 0;
  n_bits_prev = 0;
  input_bit_count = 0;
  input_bit_buffer = 0L;
  output_bit_count = 0;
  output_bit_buffer = 0L;
}

/*
** This is the hashing routine.  It tries to find a match for the prefix+char
** string in the string table.  If it finds it, the index is returned.  If
** the string is not found, the first available index in the string table is
** returned instead.
*/

int
LZW::find_match(unsigned int hash_prefix, unsigned int hash_character)
{
  int index;
  int offset;

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


/* Helper for input_code() */
unsigned int
LZW::mask(const int n_bits)
{
  if (n_bits_prev == n_bits) return b_mask;
  n_bits_prev = n_bits;

  b_mask = 0;
  for (int i = 0; i < n_bits; i++)
  {
    b_mask <<= 1;
    b_mask |= 1;
  }

  return b_mask;
}

/* Modified to read bytes in least significate order - djm */
unsigned int
LZW::input_code_gnu(unsigned char *input, int *pos, int inputlen, const int n_bits)
{
  int c;
  unsigned int      return_value;

  while (input_bit_count < n_bits)
  {
    if ( *pos == inputlen ) return MAX_VALUE;
    c = input[*pos];
    *pos = *pos + 1;
    input_bit_buffer |= c << input_bit_count;
    input_bit_count += 8;
  }

  return_value = input_bit_buffer & mask(n_bits);
  input_bit_buffer >>= n_bits;
  input_bit_count -= n_bits;

  return return_value;
}


/*
** This routine simply decodes a string from the string table, storing
** it in a buffer.  The buffer can then be output in reverse order by
** the expansion program.
*/

unsigned char *
LZW::decode_string(unsigned char *buffer, unsigned int code)
{
  int i;
  i=0;
  while (code > 255)
  {
    *buffer++ = append_character[code];
    code=prefix_code[code];
    if (i++>=MAX_CODE) {
      click_chatter("Fatal error during code expansion.\n");
    }
  }

  *buffer=code;

  return(buffer);
}

/* Modified so sends least signifigant bytes first like GNU/Linux version - djm */
void
LZW::output_code_gnu(unsigned char *output, int *pos, unsigned int code, const int n_bits)
{
  unsigned char outc;

  output_bit_buffer |= (unsigned long)code << output_bit_count;
  output_bit_count += n_bits;

//  click_chatter("Bit: %d",n_bits);

  while (output_bit_count >= 8)
  {
    outc = output_bit_buffer & 0xff;
    output[*pos] = outc;
    *pos = *pos + 1;
    output_bit_buffer >>= 8;
    output_bit_count -= 8;
  }
}

unsigned int
LZW::input_code(unsigned char *input, int *pos, int /*inputlen*/)
{
  unsigned long c;
  unsigned int return_value;

  while (input_bit_count <= 24)
  {
    c = input[*pos];
    *pos = *pos + 1;
    input_bit_buffer |= (unsigned long)c << (24-input_bit_count);
    input_bit_count += 8;
  }

  return_value=input_bit_buffer >> (32-BITS);
  input_bit_buffer <<= BITS;
  input_bit_count -= BITS;

  return(return_value);
}

void
LZW::output_code(unsigned char *output, int *pos, unsigned int code)
{
  unsigned char outc;

  output_bit_buffer |= (unsigned long) code << (32-BITS-output_bit_count);
  output_bit_count += BITS;

  while (output_bit_count >= 8)
  {
    outc = output_bit_buffer >> 24;
//    click_chatter("Out: %d",outc);
    output[*pos] = outc;
    *pos = *pos + 1;
    output_bit_buffer <<= 8;
    output_bit_count -= 8;
  }
}

/*
** This is the compression routine.  The code should be a fairly close
** match to the algorithm accompanying the article.
**
*/
//void compress(FILE *input,FILE *output)
int
LZW::encode(unsigned char *input, int inputlen, unsigned char *output, int/* max_outputlen*/)
{
  unsigned int next_code;
  unsigned int character;
  unsigned int string_code;
  unsigned int index;
  int inputpos;
  int outputpos;
  int i;

  reset_tables();

  inputpos = 0;
  outputpos = 0;

  //next_code=FIRST; GNU        /* Next code is the next available string code*/
  next_code=256;                /* Next code is the next available string code*/

  for (i=0;i<TABLE_SIZE;i++)  /* Clear out the string table before starting */
    code_value[i]=-1;

  /* output the header */
  //output[outputpos] = MAGIC_1; outputpos++;  GNU
  //output[outputpos] = MAGIC_2; outputpos++;  GNU
  //output[outputpos] = BITS | BLOCK_MODE; outputpos++; GNU

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
    index=find_match(string_code,character);/* See if the string is in */
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

      //output_code_gnu(output, &outputpos, string_code, BITS);  /* When a string is found  */
      output_code(output, &outputpos, string_code);  /* When a string is found  */

      string_code=character;            /* that is not in the table*/
    }                                   /* I output the last string*/
  }                                     /* after adding the new one*/
/*
  ** End of the main loop.
*/
//  output_code_gnu(output, &outputpos, string_code,BITS); /* Output the last code               */
//  output_code_gnu(output, &outputpos, 0,BITS);           /* This code flushes the output buffer*/
  output_code(output, &outputpos, string_code); /* Output the last code               */
  output_code(output, &outputpos, MAX_VALUE); /* Output the last code               */
  output_code(output, &outputpos, 0);           /* This code flushes the output buffer*/

  return outputpos;
}

/*
**  This is the expansion routine.  It takes an LZW format file, and expands
**  it to an output file.  The code here should be a fairly close match to
**  the algorithm in the accompanying article.
*/
//void expand(FILE *input,FILE *output)
int
LZW::decode(unsigned char *input, int inputlen, unsigned char *output, int /*max_outputlen*/)
{
  unsigned int next_code;
  unsigned int new_code;
  unsigned int old_code;
  int character;
  int counter;
  unsigned char *string;
  //int n_bits;
  //int block_mode;
  int inputpos;
  int outputpos;

  reset_tables();

  inputpos=0;
  outputpos=0;

  /* read the header - djm */
  //if (input[inputpos] != MAGIC_1) return -1;  GNU
  //inputpos++;                                 GNU
  //if (input[inputpos] != MAGIC_2) return -1;  GNU
  //inputpos++;                                 GNU


  //character = input[inputpos]; inputpos++;    GNU
  //n_bits = character & BIT_MASK;              GNU
  //block_mode = character & BLOCK_MODE;        GNU

  //next_code=FIRST;   GNU /* This is the next available code to define */
  next_code=256;           /* This is the next available code to define */
  counter=0;               /* Counter is used as a pacifier.            */

  //old_code=input_code_gnu(input, &inputpos, inputlen, n_bits);  /* Read in the first code, initialize the */
  old_code=input_code(input, &inputpos, inputlen);  /* Read in the first code, initialize the */
  character=old_code;          /* character variable, and send the first */
  output[outputpos] = old_code; outputpos++;
/*
  **  This is the main expansion loop.  It reads in characters from the LZW file
  **  until it sees the special code used to inidicate the end of the data.
*/
  //while ( ( new_code = input_code_gnu(input, &inputpos, inputlen, n_bits) ) != (MAX_VALUE))
  while ( ( new_code = input_code(input, &inputpos, inputlen) ) != (MAX_VALUE))
  {
/*
    ** This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING
    ** case which generates an undefined code.  It handles it by decoding
    ** the last code, and adding a single character to the end of the decode string.
*/
    if (new_code>=next_code)
    {
      *decode_stack=character;
      string=decode_string(decode_stack+1,old_code);
    }
/*
    ** Otherwise we do a straight decode of the new code.
*/
    else
      string=decode_string(decode_stack,new_code);
/*
    ** Now we output the decoded string in reverse order.
*/
    character=*string;
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

ELEMENT_REQUIRES(BitfieldStreamArray)
ELEMENT_PROVIDES(LZW)
