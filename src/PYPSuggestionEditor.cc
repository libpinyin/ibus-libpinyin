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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "PYPSuggestionEditor.h"
#include <assert.h>
#include "PYConfig.h"
#include "PYLibPinyin.h"
#include "PYPinyinProperties.h"

using namespace PY;

SuggestionEditor::SuggestionEditor (PinyinProperties &props,
                                    Config & config)
    : Editor (props, config),
      m_suggestion_candidates (this),
#ifdef IBUS_BUILD_LUA_EXTENSION
      m_lua_trigger_candidates (this),
      m_lua_converter_candidates (this),
#endif
      m_traditional_candidates (this, config)
{
    /* use m_text to store the prefix string. */
    m_text = "";
    m_cursor = 0;

    m_instance = LibPinyinBackEnd::instance ().allocPinyinInstance ();
}

SuggestionEditor::~SuggestionEditor (void)
{
    LibPinyinBackEnd::instance ().freePinyinInstance (m_instance);
    m_instance = NULL;
}

#ifdef IBUS_BUILD_LUA_EXTENSION
gboolean
SuggestionEditor::setLuaPlugin (IBusEnginePlugin *plugin)
{
    m_lua_trigger_candidates.setLuaPlugin (plugin);
    m_lua_converter_candidates.setLuaPlugin (plugin);
    return TRUE;
}
#endif

gboolean
SuggestionEditor::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    //IBUS_SHIFT_MASK is removed.
    modifiers &= (IBUS_CONTROL_MASK |
                  IBUS_MOD1_MASK |
                  IBUS_SUPER_MASK |
                  IBUS_HYPER_MASK |
                  IBUS_META_MASK |
                  IBUS_LOCK_MASK);
    if (modifiers)
        return FALSE;

    // handle enter here.
    if (keyval == IBUS_Return)
        return FALSE;

    //handle page/cursor up/down here.
    if (processPageKey (keyval))
        return TRUE;

    //handle label key select here.
    if (processLabelKey (keyval))
        return TRUE;

    if (processSpace (keyval))
        return TRUE;

    return FALSE;
}

gboolean
SuggestionEditor::processPageKey (guint keyval)
{
    switch (keyval) {
    case IBUS_comma:
        if (m_config.commaPeriodPage ()) {
            pageUp ();
            return TRUE;
        }
        break;
    case IBUS_minus:
        if (m_config.minusEqualPage ()) {
            pageUp ();
            return TRUE;
        }
        break;
    case IBUS_period:
        if (m_config.commaPeriodPage ()) {
            pageDown ();
            return TRUE;
        }
        break;
    case IBUS_equal:
        if (m_config.minusEqualPage ()) {
            pageDown ();
            return TRUE;
        }
        break;

    case IBUS_Up:
    case IBUS_KP_Up:
        cursorUp ();
        return TRUE;

    case IBUS_Down:
    case IBUS_KP_Down:
        cursorDown ();
        return TRUE;

    case IBUS_Page_Up:
    case IBUS_KP_Page_Up:
        pageUp ();
        return TRUE;

    case IBUS_Page_Down:
    case IBUS_KP_Page_Down:
        pageDown ();
        return TRUE;

    case IBUS_Escape:
        reset ();
        return TRUE;
    }
    return FALSE;
}

gboolean
SuggestionEditor::processLabelKey (guint keyval)
{
    switch (keyval) {
    case '1' ... '9':
        return selectCandidateInPage (keyval - '1');
        break;
    case '0':
        return selectCandidateInPage (9);
        break;
    }
    return FALSE;
}

gboolean
SuggestionEditor::processSpace (guint keyval)
{
    if (!(keyval == IBUS_space || keyval == IBUS_KP_Space))
        return FALSE;

    guint cursor_pos = m_lookup_table.cursorPos ();
    return selectCandidate (cursor_pos);
}

void
SuggestionEditor::candidateClicked (guint index, guint button, guint state)
{
    selectCandidateInPage (index);
}

gboolean
SuggestionEditor::selectCandidateInPage (guint index)
{
    guint page_size = m_lookup_table.pageSize ();
    guint cursor_pos = m_lookup_table.cursorPos ();

    if (G_UNLIKELY (index >= page_size))
        return FALSE;
    index += (cursor_pos / page_size) * page_size;

    return selectCandidate (index);
}

gboolean
SuggestionEditor::selectCandidate (guint index)
{
    if (G_UNLIKELY (index >= m_candidates.size ()))
        return FALSE;

    EnhancedCandidate & candidate = m_candidates[index];
    int action = selectCandidateInternal (candidate);

    if (action & SELECT_CANDIDATE_COMMIT) {
        Text text (candidate.m_display_string);
        commitText (text);
    }

    if (action & SELECT_CANDIDATE_UPDATE)
        update ();

    return TRUE;
}

/* Auxiliary Functions */

void
SuggestionEditor::pageUp (void)
{
    if (G_LIKELY (m_lookup_table.pageUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
SuggestionEditor::pageDown (void)
{
    if (G_LIKELY (m_lookup_table.pageDown ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
SuggestionEditor::cursorUp (void)
{
    if (G_LIKELY (m_lookup_table.cursorUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
SuggestionEditor::cursorDown (void)
{
    if (G_LIKELY (m_lookup_table.cursorDown ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
SuggestionEditor::update (void)
{
    pinyin_guess_predicted_candidates (m_instance, m_text);

    updateLookupTable ();
    updatePreeditText ();
    updateAuxiliaryText ();
}

void
SuggestionEditor::reset (void)
{
    m_text = "";
    update ();
}

void
SuggestionEditor::updateLookupTableFast (void)
{
    Editor::updateLookupTableFast (m_lookup_table, TRUE);
}

void
SuggestionEditor::updateLookupTable (void)
{
    m_lookup_table.clear ();
    m_lookup_table.setPageSize (m_config.pageSize ());
    m_lookup_table.setOrientation (m_config.orientation ());

    updateCandidates ();
    fillLookupTable ();
    if (m_lookup_table.size ()){
        Editor::updateLookupTableFast (m_lookup_table, TRUE);
    } else {
        hideLookupTable ();
        /* clean up prefix */
        m_text = "";
    }
}

gboolean
SuggestionEditor::updateCandidates (void)
{
    m_candidates.clear ();

    m_suggestion_candidates.processCandidates (m_candidates);

    if (!m_props.modeSimp ())
        m_traditional_candidates.processCandidates (m_candidates);

#ifdef IBUS_BUILD_LUA_EXTENSION
    m_lua_trigger_candidates.processCandidates (m_candidates);

    std::string converter = m_config.luaConverter ();

    if (!converter.empty ()) {
        m_lua_converter_candidates.setConverter (converter.c_str ());
        m_lua_converter_candidates.processCandidates (m_candidates);
    }
#endif

    return TRUE;
}

gboolean
SuggestionEditor::fillLookupTable ()
{
    for (guint i = 0; i < m_candidates.size (); i++) {
        EnhancedCandidate & candidate = m_candidates[i];

        Text text (candidate.m_display_string);

        /* no user candidate in suggestion editor. */
        assert (CANDIDATE_USER != candidate.m_candidate_type);

        m_lookup_table.appendCandidate (text);
    }

    return TRUE;
}

int
SuggestionEditor::selectCandidateInternal (EnhancedCandidate & candidate)
{
    switch (candidate.m_candidate_type) {
    case CANDIDATE_SUGGESTION:
        return m_suggestion_candidates.selectCandidate (candidate);

    case CANDIDATE_TRADITIONAL_CHINESE:
        return m_traditional_candidates.selectCandidate (candidate);

#ifdef IBUS_BUILD_LUA_EXTENSION
    case CANDIDATE_LUA_TRIGGER:
        return m_lua_trigger_candidates.selectCandidate (candidate);

    case CANDIDATE_LUA_CONVERTER:
        return m_lua_converter_candidates.selectCandidate (candidate);
#endif

    default:
        assert (FALSE);
    }
}

void
SuggestionEditor::updatePreeditText (void)
{
    if (G_UNLIKELY (m_preedit_text.empty ())) {
        hidePreeditText ();
        return;
    }

    StaticText preedit_text (m_preedit_text);
    Editor::updatePreeditText (preedit_text, m_cursor, TRUE);
}

void
SuggestionEditor::updateAuxiliaryText (void)
{
    if (G_UNLIKELY (m_auxiliary_text.empty ())) {
        hideAuxiliaryText ();
        return;
    }

    StaticText aux_text (m_auxiliary_text);
    Editor::updateAuxiliaryText (aux_text, TRUE);
}
