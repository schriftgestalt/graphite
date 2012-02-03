/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#pragma once

#include "inc/Main.h"

namespace graphite2 {

class Face;

class Cmap
{
public :
	virtual ~Cmap() throw() {}
	virtual uint16 operator [] (const uint32) const throw() { return 0; }
	virtual operator bool () const throw() { return false; }

	CLASS_NEW_DELETE;
};

class DirectCmap : public Cmap
{
public :
	DirectCmap(const void* cmap, size_t length);
	virtual uint16 operator [] (const uint32 usv) const throw();
	virtual operator bool () const throw();

    CLASS_NEW_DELETE;
private :
    const void *_stable,
    		   *_ctable;
};

class CmapCache : public Cmap
{
public :
	CmapCache(const void * cmapTable, size_t length);
	virtual ~CmapCache() throw();
	virtual uint16 operator [] (const uint32 usv) const throw();
	virtual operator bool () const throw();

    CLASS_NEW_DELETE;
private :
    bool m_isBmpOnly;
    uint16 ** m_blocks;
};

} // namespace graphite2
