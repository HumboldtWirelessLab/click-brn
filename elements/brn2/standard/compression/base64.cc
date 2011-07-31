#include <click/config.h>
#include <click/string.hh>
#include "base64.hh"

CLICK_DECLS

static const char base64_chars[] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/' };

static unsigned char base64_chars_find(unsigned char c) {
  if ( ( c >= 65 ) && ( c <= 90 ) ) return (c - 65);  //A-Z     -> 0 - 25
  if ( ( c >= 97 ) && ( c <= 122 ) ) return (c - 71); //a-z     -> 26 - 51
  if ( ( c >= 48 ) && ( c <= 57 ) ) return (c + 4);   //'0'-'9' -> 52 - 61
  if ( c == 43 ) return 62;                           //'+'     -> 62
  if ( c == 47 ) return 63;                           //'/'     -> 62

  click_chatter("Base64 error: %d",(int)c);
  return 255;
}

static inline bool is_base64(unsigned char c) {
  return (isalnum(c) || (c == '+') || (c == '/'));
}

Base64::Base64()
{
}

Base64::~Base64()
{
}

//std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
int
Base64::encode(unsigned char *input, int inputlen, unsigned char *output, int max_outputlen)
{

  if ( ((inputlen * 4) % 3) > max_outputlen ) return -1;

  int i = 0;
  int j = 0;
  int input_index = 0, output_index = 0, remain_inlen = inputlen;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (remain_inlen--) {
    char_array_3[i++] = input[input_index++];

    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i < 4) ; i++)
        output[output_index++] = base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i)
  {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      output[output_index++] = base64_chars[char_array_4[j]];

    while((i++ < 3))
      output[output_index++] = '=';

  }

  return output_index;
}

//std::string base64_decode(std::string const& encoded_string) {

int
Base64::decode(unsigned char *input, int inputlen, unsigned char *output, int max_outputlen)
{
  if ( ((inputlen * 3) % 4) > max_outputlen ) return -1;

  int input_index = 0, output_index = 0, remain_inlen = inputlen;
  int i = 0;
  int j = 0;
  unsigned char char_array_4[4], char_array_3[3];

  while (remain_inlen-- && ( input[input_index] != '=') && is_base64(input[input_index])) {
    char_array_4[i++] = input[input_index]; input_index++;
    if (i ==4) {
      for (i = 0; i <4; i++) {
        unsigned char f = base64_chars_find(char_array_4[i]);
        if ( f == 255 ) click_chatter("Foo: %d",(int)char_array_4[i]);
        char_array_4[i] = f;
      }

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        output[output_index++] = char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j <4; j++)
      char_array_4[j] = 0;

    for (j = 0; j <4; j++) {
      unsigned char f = base64_chars_find(char_array_4[j]);
      if ( f == 255 ) click_chatter("Foo: %d",(int)char_array_4[j]);
      char_array_4[j] = f;
    }

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) output[output_index++] = char_array_3[j];
  }

  return output_index;
}

void
Base64::base64_test() {
  String foo = "This is a basic Base64 test.";

  unsigned char test_array[5000];
  unsigned char test_2_array[5000];

  int s = encode((unsigned char*)foo.data(),foo.length(),test_array,5000);
  int s2 = decode(test_array,s,test_2_array,5000);

  test_array[s] = '\0';
  test_2_array[s2] = '\0';
  click_chatter("Base64: %s",test_array);
  String foo2 = String((char*)test_2_array);
  click_chatter("->%s<-",foo2.c_str());
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(Base64)
