/* Copyright (C) 2008 Ulf Hermann
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
 */

#ifndef HASHSET_HH_
#define HASHSET_HH_

#include<click/hashmap.hh>

CLICK_DECLS



template<class T>
class HashSet : public HashMap<T,T> {
public:
	bool insert(T item) {return HashMap<T,T>::insert(item,item);}
	bool contains(const T & item) const {return find_pair(item);}
	const T & front() {return HashMap<T,T>::begin().value();}
};


CLICK_ENDDECLS

#endif /*HASHSET_HH_*/
