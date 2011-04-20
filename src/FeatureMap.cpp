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
    If not, write to the Free Software Foundation, Inc., 59 Temple Place, 
    Suite 330, Boston, MA 02111-1307, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#include <string.h>

#include "Main.h"
#include "FeatureMap.h"
#include "FeatureVal.h"
#include "graphite2/Font.h"
#include "XmlTraceLog.h"
#include "TtfUtil.h"
//#include <algorithm>
#include <stdlib.h>
#include "Face.h"

#define ktiFeat MAKE_TAG('F','e','a','t')
#define ktiSill MAKE_TAG('S','i','l','l')


using namespace graphite2;

static int cmpNameAndFeatures(const void *a, const void *b) { return (*(NameAndFeatureRef *)a < *(NameAndFeatureRef *)b 
                                                                        ? -1 : (*(NameAndFeatureRef *)b < *(NameAndFeatureRef *)a 
                                                                                    ? 1 : 0)); }

bool FeatureMap::readFeats(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable, const Face* pFace)
{
    size_t lFeat;
    const byte *pFeat = reinterpret_cast<const byte *>((*getTable)(appFaceHandle, ktiFeat, &lFeat));
    const byte *pOrig = pFeat;
    uint16 *defVals=0;
    uint32 version;
    if (!pFeat) return true;
    if (lFeat < 12) return false;

    version = read32(pFeat);
    if (version < 0x00010000) return false;
    m_numFeats = read16(pFeat);
    read16(pFeat);
    read32(pFeat);
    if (m_numFeats * 16U + 12 > lFeat) { m_numFeats = 0; return false; }		//defensive
    if (m_numFeats)
    {
        m_feats = new FeatureRef[m_numFeats];
        m_pNamedFeats = new NameAndFeatureRef[m_numFeats];
        defVals = gralloc<uint16>(m_numFeats);
    }
    byte currIndex = 0;
    byte currBits = 0;     //to cause overflow on first Feature

#ifndef DISABLE_TRACING
    if (XmlTraceLog::get().active())
    {
        XmlTraceLog::get().openElement(ElementFeatures);
        XmlTraceLog::get().addAttribute(AttrMajor, version >> 16);
        XmlTraceLog::get().addAttribute(AttrMinor, version & 0xFFFF);
        XmlTraceLog::get().addAttribute(AttrNum, m_numFeats);
    }
#endif
    for (int i = 0; i < m_numFeats; i++)
    {
        uint32 name;
        if (version < 0x00020000)
            name = read16(pFeat);
        else
            name = read32(pFeat);
        uint16 numSet = read16(pFeat);
        uint32 offset;
        if (version < 0x00020000)
            offset = read32(pFeat);
        else
        {
            read16(pFeat);
            offset = read32(pFeat);
        }
        uint16 flags = read16(pFeat);
        uint16 uiName = read16(pFeat);
        const byte *pSet = pOrig + offset;
        uint16 maxVal = 0;

#ifndef DISABLE_TRACING
        if (XmlTraceLog::get().active())
        {
            XmlTraceLog::get().openElement(ElementFeature);
            XmlTraceLog::get().addAttribute(AttrIndex, i);
            XmlTraceLog::get().addAttribute(AttrNum, name);
            XmlTraceLog::get().addAttribute(AttrFlags, flags);
            XmlTraceLog::get().addAttribute(AttrLabel, uiName);
        }
#endif
        if (offset + numSet * 4 > lFeat) return false;
        FeatureSetting *uiSet = gralloc<FeatureSetting>(numSet);
        for (int j = 0; j < numSet; j++)
        {
            int16 val = read16(pSet);
            if (val > maxVal) maxVal = val;
            if (j == 0) defVals[i] = val;
            uint16 label = read16(pSet);
            ::new(uiSet + j) FeatureSetting(label, val);
#ifndef DISABLE_TRACING
            if (XmlTraceLog::get().active())
            {
                XmlTraceLog::get().openElement(ElementFeatureSetting);
                XmlTraceLog::get().addAttribute(AttrIndex, j);
                XmlTraceLog::get().addAttribute(AttrValue, val);
                XmlTraceLog::get().addAttribute(AttrLabel, label);
                if (j == 0) XmlTraceLog::get().addAttribute(AttrDefault, defVals[i]);
                XmlTraceLog::get().closeElement(ElementFeatureSetting);
            }
#endif
        }
        uint32 mask = 1;
        byte bits = 0;
        // check for an empty legacy lang feature at the end with ID=1 and ignore
        if ((version <= 0x20000) && (name == 1) && (numSet == 0) && (i + 1 == m_numFeats))
        {
            --m_numFeats;
#ifndef DISABLE_TRACING
            XmlTraceLog::get().closeElement(ElementFeature);
#endif
            break;
        }
        for (bits = 0; bits < 32; bits++, mask <<= 1)
        {
            if (mask > maxVal)
            {
                if (bits + currBits > 32)
                {
                    currIndex++;
                    currBits = 0;
                    mask = 2;
                }
                ::new (m_feats + i) FeatureRef (currBits, currIndex,
                                               (mask - 1) << currBits, flags,
                                               name, uiName, numSet, uiSet, pFace);
                currBits += bits;
                break;
            }
        }
#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementFeature);
#endif
    }
    m_defaultFeatures = new Features(currIndex + 1, *this);
    for (int i = 0; i < m_numFeats; i++)
    {
	m_feats[i].applyValToFeature(defVals[i], *m_defaultFeatures);
        m_pNamedFeats[i] = m_feats+i;
    }
    
    free(defVals);

    qsort(m_pNamedFeats, m_numFeats, sizeof(NameAndFeatureRef), &cmpNameAndFeatures);

#ifndef DISABLE_TRACING
    XmlTraceLog::get().closeElement(ElementFeatures);
#endif

    return true;
}

bool SillMap::readFace(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable, const Face* pFace)
{
    if (!m_FeatureMap.readFeats(appFaceHandle, getTable, pFace)) return false;
    if (!readSill(appFaceHandle, getTable)) return false;
    return true;
}


bool SillMap::readSill(const void* appFaceHandle/*non-NULL*/, gr_get_table_fn getTable)
{
    size_t lSill;
    const byte *pSill = reinterpret_cast<const byte *>(((*getTable)(appFaceHandle, ktiSill, &lSill)));
    const byte *pBase = pSill;

    if (!pSill) return true;
    if (lSill < 12) return false;
    if (read32(pSill) != 0x00010000UL) return false;
    m_numLanguages = read16(pSill);
    m_langFeats = new LangFeaturePair[m_numLanguages];
    if (!m_langFeats || !m_FeatureMap.m_numFeats) { m_numLanguages = 0; return true; }        //defensive

    pSill += 6;     // skip the fast search
    if (lSill < m_numLanguages * 8U + 12) return false;

    for (int i = 0; i < m_numLanguages; i++)
    {
        uint32 langid = read32(pSill);
        uint16 numSettings = read16(pSill);
        uint16 offset = read16(pSill);
        if (offset + 8U * numSettings > lSill && numSettings > 0) return false;
        Features* feats = new Features(*m_FeatureMap.m_defaultFeatures);
        const byte *pLSet = pBase + offset;

        for (int j = 0; j < numSettings; j++)
        {
            uint32 name = read32(pLSet);
            uint16 val = read16(pLSet);
            pLSet += 2;
            const FeatureRef* pRef = m_FeatureMap.findFeatureRef(name);
            if (pRef)
                pRef->applyValToFeature(val, *feats);
        }
        //std::pair<uint32, Features *>kvalue = std::pair<uint32, Features *>(langid, feats);
        //m_langMap.insert(kvalue);
        m_langFeats[i].m_lang = langid;
        m_langFeats[i].m_pFeatures = feats;
    }
    return true;
}


Features* SillMap::cloneFeatures(uint32 langname/*0 means default*/) const
{
    if (langname)
    {
        // the number of languages in a font is usually small e.g. 8 in Doulos
        // so this loop is not very expensive
        for (uint16 i = 0; i < m_numLanguages; i++)
        {
            if (m_langFeats[i].m_lang == langname)
                return new Features(*m_langFeats[i].m_pFeatures);
        }
    }
    return new Features (*m_FeatureMap.m_defaultFeatures);
}



const FeatureRef *FeatureMap::findFeatureRef(uint32 name) const
{
    NameAndFeatureRef *it;
    
    for (it = m_pNamedFeats; it < m_pNamedFeats + m_numFeats; ++it)
        if (it->m_name == name)
            return it->m_pFRef;
    return NULL;
}

bool FeatureRef::applyValToFeature(uint16 val, Features & pDest) const 
{ 
    if (val>m_max || !m_pFace)
      return false;
    if (pDest.m_pMap==NULL)
      pDest.m_pMap = &m_pFace->theSill().theFeatureMap();
    else
      if (pDest.m_pMap!=&m_pFace->theSill().theFeatureMap())
        return false;       //incompatible
    pDest.reserve(m_index);
    pDest[m_index] &= ~m_mask;
    pDest[m_index] |= (uint32(val) << m_bits);
    return true;
}

uint16 FeatureRef::getFeatureVal(const Features& feats) const
{ 
  if (m_index < feats.size() && &m_pFace->theSill().theFeatureMap()==feats.m_pMap) 
    return (feats[m_index] & m_mask) >> m_bits; 
  else
    return 0;
}


