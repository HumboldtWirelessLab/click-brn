/* Copyright (C) 2005 BerlinRoofNet Lab
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA. 
 *
 * For additional licensing options, consult http://www.BerlinRoofNet.de 
 * or contact brn@informatik.hu-berlin.de. 
 */

#include <click/config.h>
#include <click/glue.hh>
#include <click/string.hh>

#include "binarymatrix.hh"


CLICK_DECLS;

BinaryMatrix::BinaryMatrix(size_t n)
{
  dimension = n;
  matrix = new bool[dimension * dimension];
  setZeroMatrix();
}

BinaryMatrix::BinaryMatrix(const BinaryMatrix &bm)
{
  dimension = bm.dimension;
  matrix = new bool[dimension * dimension]; //TODO: copy matrix
  setZeroMatrix();
}

BinaryMatrix::~BinaryMatrix()
{
  delete[](matrix);
}

void BinaryMatrix::setTrue(size_t row, size_t col)
{
  matrix[row * dimension + col] = true;
}

void BinaryMatrix::setZeroMatrix()
{
  for (size_t col = 0; col < dimension; col++)
    for (size_t row = 0; row < dimension; row++)
      matrix[row * dimension + col] = 0;
}

void BinaryMatrix::runMarshallAlgo()
{
  for (size_t col = 0; col < dimension; col++)
  {
    for (size_t row = 0; row < dimension; row++)
    {
      if (matrix[row * dimension + col] == true)
      {
        for (size_t j = 0; j < dimension; j++)
        {
          matrix[row * dimension + j] =
              matrix[row * dimension + j] || matrix[col * dimension + j];
        }
      }
    }
  }
}

void BinaryMatrix::print(const char *nodeAddr)
{
  click_chatter("<Matrix node='%s' >", nodeAddr);
  for (size_t i = 0; i < dimension; i++)
  {
    String row_text = "";
    for (size_t k = 0; k < dimension; k++)
    {
      if (k > 0)
        row_text += ", ";
      row_text += matrix[i * dimension + k] ? "1" : "0";
    }
    click_chatter("  <MatrixRow num='%d'>%s</MatrixRow>", i, row_text.c_str());
  }
  click_chatter("</Matrix>");
}

bool BinaryMatrix::isOneMatrix()
{
  for (size_t col = 0; col < dimension; col++)
  {
    for (size_t row = 0; row < dimension; row++)
    {
      if (matrix[row * dimension + col] == false)
        return (false);
    }
  }
  return (true);
}


CLICK_ENDDECLS
ELEMENT_PROVIDES(DibadawnBinaryMatrix)
