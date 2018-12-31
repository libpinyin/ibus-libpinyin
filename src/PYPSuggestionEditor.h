/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2018 Peng Wu <alexepico@gmail.com>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __PY_LIB_PINYIN_SUGGESTION_EDITOR_
#define __PY_LIB_PINYIN_SUGGESTION_EDITOR_

#include <pinyin.h>
#include "PYEditor.h"
#include "PYLookupTable.h"
#include "PYPSuggestionCandidates.h"
#include "PYPTradCandidates.h"

#ifdef IBUS_BUILD_LUA_EXTENSION
#include "PYPLuaTriggerCandidates.h"
#include "PYPLuaConverterCandidates.h"
#endif

namespace PY {

class SuggestionEditor : public Editor {
    friend class SuggestionCandidates;

public:
    SuggestionEditor (PinyinProperties &props, Config & config);
    virtual ~SuggestionEditor ();

    virtual gboolean processKeyEvent (guint keyval, guint keycode, guint modifers);
    virtual void pageUp (void);
    virtual void pageDown (void);
    virtual void cursorUp (void);
    virtual void cursorDown (void);
    virtual void update (void);
    virtual void reset (void);
    virtual void candidateClicked (guint index, guint button, guint state);

#ifdef IBUS_BUILD_LUA_EXTENSION
    gboolean setLuaPlugin (IBusEnginePlugin *plugin);
#endif

protected:
    virtual int selectCandidateInternal (EnhancedCandidate & candidate);

private:
    void updateLookupTable (void);
    void updateLookupTableFast ();
    gboolean updateCandidates ();
    gboolean fillLookupTable ();
    void updatePreeditText (void);
    void updateAuxiliaryText (void);

    gboolean selectCandidateInPage (guint index);
    gboolean selectCandidate (guint index);

    gboolean processSpace (guint keyval);
    gboolean processLabelKey (guint keyval);
    gboolean processPageKey (guint keyval);

private:
    /* variables */
    LookupTable m_lookup_table;

    String m_preedit_text;
    String m_auxiliary_text;

    /* use LibPinyinBackEnd here. */
    pinyin_instance_t           *m_instance;

    /* use EnhancedCandidates here. */
    std::vector<EnhancedCandidate> m_candidates;

    /* several EnhancedCandidates providers. */
    SuggestionCandidates m_suggestion_candidates;

#ifdef IBUS_BUILD_LUA_EXTENSION
    LuaTriggerCandidates m_lua_trigger_candidates;
    LuaConverterCandidates m_lua_converter_candidates;
#endif

    TraditionalCandidates m_traditional_candidates;
};

};

#endif
