// Lempel-Ziv-Welch codec
// ----------------------
// This program is released under the GPL license.
// -----------------------------------------------

#ifndef LEMPEL_ZIV_WELCH_HH
#define LEMPEL_ZIV_WELCH_HH

#include <click/vector.hh>
#include "elements/brn2/standard/bitfield/bitfieldstreamarray.hh"

CLICK_DECLS

class LZWCodec
{
 public:

  // The string element:
  class CodeString
  {
   public:
    unsigned prefixIndex;
    // First CodeString using this CodeString as prefix:
    unsigned first;
    // Next CodeStrings using the same prefixIndex as this one:
    unsigned nextLeft, nextRight;

    unsigned char k;

    CodeString(unsigned char newByte = 0, unsigned pI = ~0U):
               prefixIndex(pI), first(~0U),
               nextLeft(~0U), nextRight(~0U),
               k(newByte) {}
   };

  class Dictionary
  {
    //Vector<CodeString> table;
    CodeString *table;
    unsigned codeStart, newCodeStringIndex;
    Vector<unsigned char> decodedString;

      // Returns ~0U if c didn't already exist, else the index to the
      // existing CodeString:
    unsigned add(CodeString& c)
    {
      if(c.prefixIndex == ~0U) return c.k;

      unsigned index = table[c.prefixIndex].first;
      if(index == ~0U)
        table[c.prefixIndex].first = newCodeStringIndex;
      else
      {
        while(true)
        {
          if(c.k == table[index].k) return index;
          if(c.k < table[index].k)
          {
            const unsigned next = table[index].nextLeft;
            if(next == ~0U)
            {
              table[index].nextLeft = newCodeStringIndex;
              break;
            }
            index = next;
          }
          else
          {
            const unsigned next = table[index].nextRight;
            if(next == ~0U)
            {
              table[index].nextRight = newCodeStringIndex;
              break;
            }
            index = next;
          }
        }
      }
      table[newCodeStringIndex++] = c;

      return ~0U;
    }

    void fillDecodedString(unsigned code)
    {
      decodedString.clear();
      while(code != ~0U)
      {
        const CodeString& cs = table[code];
        decodedString.push_back(cs.k);
        code = cs.prefixIndex;
      }
    }


   public:
    Dictionary(unsigned maxBits, unsigned _codeStart):
//      table(1<<maxBits),
      codeStart(_codeStart), newCodeStringIndex(_codeStart)
    {
      table = new CodeString[1<<maxBits];
      for(unsigned i = 0; i < codeStart; ++i)
        table[i].k = i;
    }

    ~Dictionary() {
      delete table;
    }

    bool searchCodeString(CodeString& c)
    {
      unsigned index = add(c);
      if(index != ~0U)
      {
        c.prefixIndex = index;
        return true;
      }
      return false;
    }

    void decode(unsigned oldCode, unsigned code, BitfieldStreamArray *outStream, const unsigned char* byteMap)
    {
      const bool exists = code < newCodeStringIndex;

      if (exists) fillDecodedString(code);
      else fillDecodedString(oldCode);

      for(size_t i = decodedString.size(); i > 0;)
        outStream->writeBits(byteMap[decodedString[--i]],8);

      if (!exists) outStream->writeBits(byteMap[decodedString.back()],8);

      table[newCodeStringIndex].prefixIndex = oldCode;
      table[newCodeStringIndex++].k = decodedString.back();
    }

    unsigned size() const { return newCodeStringIndex; }

    void reset()
    {
      newCodeStringIndex = codeStart;
      for(unsigned i = 0; i < codeStart; ++i)
        table[i] = CodeString(i);
    }
  };

 public:

   // The encoder will test all max bitsizes from maxBits1 to maxBits2
    // if maxBits1 < maxBits2. In order to specify only one max bitsize
    // leave maxBits2 to 0.
    LZWCodec(unsigned maxBits1=16, unsigned maxBits2=0, bool verbose=false, bool printTime = false);
    ~LZWCodec();

    int encode(unsigned char *input, int inputlen, unsigned char *encoded, int max_encodedlen);
    int decode(unsigned char *encoded, int encodedlen, unsigned char *decoded, int max_decodedlen);

 private:
    bool verbose, printTime;
    unsigned maxBits1, maxBits2;
};

CLICK_ENDDECLS
#endif
