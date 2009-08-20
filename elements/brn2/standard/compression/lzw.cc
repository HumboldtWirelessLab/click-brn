#include <click/config.h>
#include "lzw.hh"
#include "elements/brn2/standard/bitfield/bitfieldstreamarray.hh"

CLICK_DECLS

LZWCodec::LZWCodec(unsigned maxBits1, unsigned maxBits2, bool verbose, bool printTime):
  verbose(verbose), printTime(printTime),
  maxBits1(maxBits1), maxBits2(maxBits2)
{}

LZWCodec::~LZWCodec()
{}

// Maps bytes in the input to the lowest possible values:
unsigned calculateByteMap( unsigned char *input, int inputlen, unsigned *byteMap)
{
  const size_t size = inputlen;
  bool usedByte[256];

  for(unsigned i = 0; i < 256; ++i)
  {
    usedByte[i] = false;
    byteMap[i] = 255;
  }

  for(size_t i = 0; i < size; ++i) usedByte[input[i]] = true;

  unsigned byteMapCount = 0;
  for(unsigned i = 0; i < 256; ++i)
    if (usedByte[i]) byteMap[i] = byteMapCount++;

  return byteMapCount;
}

// Calculates the minimum amount of bits required to store the specified
// value:
unsigned requiredBits(unsigned value)
{
  unsigned bits = 1;
  while((value >>= 1) > 0) ++bits;
  return bits;
}

int LZW_Encode(unsigned char *input, int inputlen, unsigned char *encoded, int max_encodedlen, unsigned maxBits)
{
  assert(maxBits < 32);
  const size_t size = inputlen;

  unsigned byteMap[256];
  const unsigned byteMapSize = calculateByteMap(input, inputlen, byteMap);
  const bool mapped = byteMapSize < 256;
  const unsigned eoiCode = byteMapSize;
  const unsigned codeStart = byteMapSize+1;
  const unsigned minBits = requiredBits(codeStart);

  BitfieldStreamArray bfas(encoded, max_encodedlen);

  if (maxBits < minBits) maxBits = minBits;

  bfas.writeBits((unsigned char)maxBits,8);
  bfas.writeBits((unsigned char)byteMapSize,8);

  if(mapped)
    for(unsigned i = 0; i < 256; ++i)
      if(byteMap[i] < 255) bfas.writeBits((unsigned char)i,8);

  LZWCodec::Dictionary dictionary(maxBits, codeStart);
  LZWCodec::CodeString currentString;

  unsigned currentBits = minBits;
  unsigned nextBitIncLimit = (1 << minBits) - 1;

  for(size_t i = 0; i < size; ++i)
  {
    currentString.k = byteMap[input[i]];

    if (!dictionary.searchCodeString(currentString))
    {
      bfas.writeBits(currentString.prefixIndex, currentBits);
      currentString.prefixIndex = currentString.k;

      if (dictionary.size() == nextBitIncLimit)
      {
        if (currentBits == maxBits)
        {
          currentBits = minBits;
          dictionary.reset();
        }
        else
          ++currentBits;

        nextBitIncLimit = (1 << currentBits) - 1;
      }
    }
  }

  bfas.writeBits(currentString.prefixIndex, currentBits);

  if ( dictionary.size() == nextBitIncLimit-1 ) ++currentBits;
  bfas.writeBits(eoiCode, currentBits);

  int resultlen = bfas.get_position();

  bfas.reset();

  int foo;
  for ( int k = 0; k < (resultlen/8); k++ ) {
    foo = bfas.readBits(8);
    click_chatter("G: %d",foo);
  }
  return (resultlen/8) ;
}

int
LZWCodec::encode(unsigned char *input, int inputlen, unsigned char *encoded, int max_encodedlen)
{
  if (maxBits2 < maxBits1) maxBits2 = maxBits1;

  unsigned bestBits = 0;
  size_t bestSize = 0;
  int len;
  unsigned maxBits;

  for( maxBits = maxBits1; maxBits <= maxBits2; ++maxBits)
  {
    len = LZW_Encode(input, inputlen, encoded, max_encodedlen, maxBits);

    if ( ( len != -1 ) && ( maxBits == maxBits1 || len <= (int)bestSize) )
    {
      bestBits = maxBits;
      bestSize = len;
    }

    if ( len < (int)(1U << maxBits)) break;
  }

  if ( bestBits != maxBits2 ) len = LZW_Encode(input, inputlen, encoded, max_encodedlen, maxBits);

  return len;
}

int
LZWCodec::decode(unsigned char *encoded, int encodedlen, unsigned char *decoded, int max_decodedlen)
{
  unsigned char byteMap[256];
  const unsigned maxBits = encoded[0];
  const unsigned byteMapSize = (encoded[1] == 0 ? 256 : encoded[1]);
  const unsigned eoiCode = byteMapSize;
  const unsigned codeStart = byteMapSize+1;
  const unsigned minBits = requiredBits(codeStart);

  if(byteMapSize < 256)
      for(unsigned i = 0; i < byteMapSize; ++i) byteMap[i] = encoded[i+2];
  else
      for(unsigned i = 0; i < 256; ++i) byteMap[i] = (unsigned char)i;

  Dictionary dictionary(maxBits, codeStart);
  int startindex = encoded[1]==0 ? 2 : 2+byteMapSize;
  BitfieldStreamArray bfas(&(encoded[startindex]), encodedlen-startindex );
  BitfieldStreamArray bfasout(decoded, max_decodedlen);

  while(true)
  {
    dictionary.reset();
    unsigned currentBits = minBits;
    unsigned nextBitIncLimit = (1 << minBits) - 2;

    unsigned code = bfas.readBits(currentBits);
    if(code == eoiCode) return bfasout.get_position();

    bfasout.writeBits(byteMap[code],8);
    unsigned oldCode = code;

    while(true)
    {
      code = bfas.readBits(currentBits);
      if(code == eoiCode) return bfasout.get_position();

      dictionary.decode(oldCode, code, &bfasout, byteMap);
      if(dictionary.size() == nextBitIncLimit)
      {
        if(currentBits == maxBits)  break;
        else  ++currentBits;

        nextBitIncLimit = (1 << currentBits) - 2;
      }

      oldCode = code;
    }
  }
}

CLICK_ENDDECLS

ELEMENT_REQUIRES(BitfieldStreamArray)
ELEMENT_PROVIDES(LZWCodec)
