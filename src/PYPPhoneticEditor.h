/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2011 Peng Wu <alexepico@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#ifndef __PY_LIB_PINYIN_BASE_EDITOR_H_
#define __PY_LIB_PINYIN_BASE_EDITOR_H_

#include "lua-plugin.h"

#include <vector>
#include <pinyin.h>
#include "PYLookupTable.h"
#include "PYEditor.h"
#include "PYPEnhancedCandidates.h"
#include "PYPLibPinyinCandidates.h"
#include "PYPTradCandidates.h"

#ifdef IBUS_BUILD_LUA_EXTENSION
#include "PYPLuaTriggerCandidates.h"
#include "PYPLuaConverterCandidates.h"
#endif

#ifdef ENABLE_CLOUD_INPUT_MODE
#include "PYPCloudCandidates.h"
#endif

namespace PY {

class PhoneticEditor : public Editor {
    friend class LibPinyinCandidates;
    friend class CloudCandidates;

public:
    PhoneticEditor (PinyinProperties & props, Config & config);
    virtual ~PhoneticEditor ();

public:
    /* virtual functions */
    virtual void pageUp (void);
    virtual void pageDown (void);
    virtual void cursorUp (void);
    virtual void cursorDown (void);
    virtual void update (void);
    virtual void reset (void);
    virtual void candidateClicked (guint index, guint button, guint state);
    virtual gboolean processKeyEvent (guint keyval, guint keycode, guint modifiers);
    virtual gboolean processSpace (guint keyval, guint keycode, guint modifiers);
    virtual gboolean processFunctionKey (guint keyval, guint keycode, guint modifiers);
    virtual void updateLookupTable ();
    virtual void updateLookupTableFast ();
    virtual gboolean updateCandidates ();
    virtual gboolean fillLookupTable ();
    virtual void commit (const gchar *str) = 0;

    gboolean setLuaPlugin (IBusEnginePlugin *plugin);

protected:
    virtual int selectCandidateInternal (EnhancedCandidate & candidate);
    gboolean selectCandidate (guint i);
    gboolean selectCandidateInPage (guint i);

    void commit () { selectCandidate (0); }

    guint getPinyinCursor (void);
    guint getLookupCursor (void);

    /* inline functions */

    /* pure virtual functions */
    virtual gboolean insert (gint ch) = 0;
    virtual gboolean removeCharBefore (void);
    virtual gboolean removeCharAfter (void);
    virtual gboolean removeWordBefore (void);
    virtual gboolean removeWordAfter (void);
    virtual gboolean moveCursorLeft (void);
    virtual gboolean moveCursorRight (void);
    virtual gboolean moveCursorLeftByWord (void);
    virtual gboolean moveCursorRightByWord (void);
    virtual gboolean moveCursorToBegin (void);
    virtual gboolean moveCursorToEnd (void);
    virtual void updateAuxiliaryText (void) = 0;
    virtual void updatePreeditText (void) = 0;
    virtual void updatePinyin (void) = 0;

    guint getCursorLeftByWord (void);
    guint getCursorRightByWord (void);


    /* varibles */
    guint                       m_pinyin_len;
    LookupTable                 m_lookup_table;
    String                      m_buffer;

    /* use LibPinyinBackEnd here. */
    pinyin_instance_t           *m_instance;

    /* use EnhancedCandidates here. */
    std::vector<EnhancedCandidate> m_candidates;

    /* several EnhancedCandidates providers. */
    LibPinyinCandidates m_libpinyin_candidates;

#ifdef IBUS_BUILD_LUA_EXTENSION
    LuaTriggerCandidates m_lua_trigger_candidates;
    LuaConverterCandidates m_lua_converter_candidates;
#endif

    TraditionalCandidates m_traditional_candidates;
    
#ifdef ENABLE_CLOUD_INPUT_MODE
    CloudCandidates m_cloud_candidates;
#endif
};

};

#endif
