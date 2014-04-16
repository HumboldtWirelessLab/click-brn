#include <click/config.h>
#include <click/confparse.hh>

#include "fixpointnumber.hh"

CLICK_DECLS

FixPointNumber::FixPointNumber(): number(0) {}

FixPointNumber::~FixPointNumber() {}

FixPointNumber::FixPointNumber(int n): number(n) {}

FixPointNumber::FixPointNumber(String n) {
  fromString(n);
}

int
FixPointNumber::fromString(String n) {

  int pointpos = n.find_left('.',0);

  if ( pointpos == -1 ) {       //no point
    int value;
    cp_integer(n, &value);
    number = value * PREFACTOR;
  } else {
    String prespeed = n.substring(0, pointpos);

    int nonnullpos = pointpos+1;
    for (; (n.c_str()[nonnullpos] == '0') && ( nonnullpos < n.length() ); nonnullpos++);

    if ( nonnullpos == n.length() ) nonnullpos = n.length() - 1; //if only zeros, take one of them
    String postspeed = n.substring(nonnullpos,n.length());

    int prei, posti;
    cp_integer(prespeed, &prei);
    cp_integer(postspeed, &posti);

    number = prei * PREFACTOR + sizeup(posti,(n.length() - pointpos));
  }

  return 0;
}

String
FixPointNumber::unparse() const {
  StringAccum sa;

  sa << number / PREFACTOR << ".";
  int showprep = number % PREFACTOR;
  if ((showprep != 0) && (showprep < (PREFACTOR/10)))
    for (int s = showprep; s < (PREFACTOR/10); s *= 10) sa << "0";
  if ( showprep < 0 ) showprep *= -1;
  sa << showprep;

  return sa.take_string();
}

int
FixPointNumber::sizeup(int i, int digest) {
  if ( i <= 0 ) return i;
  while ( digest <= 6 ) {
    i *= 10;
    digest++;
  }
  return i;
}

CLICK_ENDDECLS

ELEMENT_PROVIDES(FixPointNumber)
