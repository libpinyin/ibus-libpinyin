/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2011 Peng Wu <alexepico@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PYPPhoneticEditor.h"
#include <assert.h>
#include "PYConfig.h"
#include "PYPinyinProperties.h"

using namespace PY;

/* init static members */
PhoneticEditor::PhoneticEditor (PinyinProperties &props,
                                Config &config)
    : Editor (props, config),
    m_pinyin_len (0),
    m_lookup_table (m_config.pageSize ()),
    m_libpinyin_candidates (this),
#ifdef IBUS_BUILD_LUA_EXTENSION
    m_lua_trigger_candidates (this),
    m_lua_converter_candidates (this),
#endif
    m_emoji_candidates (this),
#ifdef ENABLE_CLOUD_INPUT_MODE
    m_cloud_candidates(this),
#endif
    m_traditional_candidates (this, config)
{
}

PhoneticEditor::~PhoneticEditor (){
}

#ifdef IBUS_BUILD_LUA_EXTENSION
gboolean
PhoneticEditor::setLuaPlugin (IBusEnginePlugin *plugin)
{
    m_lua_trigger_candidates.setLuaPlugin (plugin);
    m_lua_converter_candidates.setLuaPlugin (plugin);
    return TRUE;
}
#endif

gboolean
PhoneticEditor::processSpace (guint keyval, guint keycode,
                              guint modifiers)
{
    if (!m_text)
        return FALSE;
    if (cmshm_filter (modifiers) != 0)
        return TRUE;

    if (m_lookup_table.size () != 0) {
        selectCandidate (m_lookup_table.cursorPos ());
        update ();
    }
    else {
        commit ();
    }

    return TRUE;
}

gboolean
PhoneticEditor::processFunctionKey (guint keyval, guint keycode, guint modifiers)
{
    if (m_text.empty ())
        return FALSE;

    /* ignore numlock */
    modifiers = cmshm_filter (modifiers);

    if (modifiers != 0 && modifiers != IBUS_CONTROL_MASK)
        return TRUE;

    /* process some cursor control keys */
    if (modifiers == 0) {  /* no modifiers. */
        switch (keyval) {
        case IBUS_Return:
        case IBUS_KP_Enter: {
            Text text (m_text.c_str ());
            commitText (text);
            reset ();
            return TRUE;
        }

        case IBUS_BackSpace:
            removeCharBefore ();
            return TRUE;

        case IBUS_Delete:
        case IBUS_KP_Delete:
            removeCharAfter ();
            return TRUE;

        case IBUS_Left:
        case IBUS_KP_Left:
            moveCursorLeft ();
            return TRUE;

        case IBUS_Right:
        case IBUS_KP_Right:
            moveCursorRight ();
            return TRUE;

        case IBUS_Home:
        case IBUS_KP_Home:
            moveCursorToBegin ();
            return TRUE;

        case IBUS_End:
        case IBUS_KP_End:
            moveCursorToEnd ();
            return TRUE;

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
        case IBUS_Tab:
            pageDown ();
            return TRUE;

        case IBUS_Escape:
            reset ();
            return TRUE;
        default:
            return TRUE;
        }
    } else { /* ctrl key pressed. */
        switch (keyval) {
        case IBUS_BackSpace:
            removeWordBefore ();
            return TRUE;

        case IBUS_Delete:
        case IBUS_KP_Delete:
            removeWordAfter ();
            return TRUE;

        case IBUS_Left:
        case IBUS_KP_Left:
            moveCursorLeftByWord ();
            return TRUE;

        case IBUS_Right:
        case IBUS_KP_Right:
            moveCursorRightByWord ();
            return TRUE;

        /* remove user phrase */
        case IBUS_D:
            {
                guint index = m_lookup_table.cursorPos ();
                if (index < m_candidates.size () &&
                    removeCandidateInternal (m_candidates[index])) {
                    updatePinyin ();
                    update ();
                }
                return TRUE;
            }
        default:
            return TRUE;
        }
    }
    return TRUE;
}

gboolean
PhoneticEditor::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    return FALSE;
}

void
PhoneticEditor::updateLookupTableFast (void)
{
    Editor::updateLookupTableFast (m_lookup_table, TRUE);
}

void
PhoneticEditor::updateLookupTable (void)
{
    m_lookup_table.clear ();

    updateCandidates ();
    fillLookupTable ();
    if (m_lookup_table.size()) {
        Editor::updateLookupTable (m_lookup_table, TRUE);
    } else {
        hideLookupTable ();
    }
}

gboolean
PhoneticEditor::updateCandidates (void)
{
    m_candidates.clear ();

    m_libpinyin_candidates.processCandidates (m_candidates);

    if (m_config.emojiCandidate ())
        m_emoji_candidates.processCandidates (m_candidates);

#ifdef IBUS_BUILD_LUA_EXTENSION
    m_lua_trigger_candidates.processCandidates (m_candidates);

    std::string converter = m_config.luaConverter ();

    if (!converter.empty ()) {
        m_lua_converter_candidates.setConverter (converter.c_str ());
        m_lua_converter_candidates.processCandidates (m_candidates);
    }
#endif

#ifdef ENABLE_CLOUD_INPUT_MODE
    /* keep me behind the other kinds of candidates which are inserted after n-gram candidates */
    if(m_config.enableCloudInput ())
        m_cloud_candidates.processCandidates (m_candidates);
#endif

    if (!m_props.modeSimp ())
        m_traditional_candidates.processCandidates (m_candidates);

    return TRUE;
}

gboolean
PhoneticEditor::fillLookupTable (void)
{
    String word;
    for (guint i = 0; i < m_candidates.size (); i++) {
        EnhancedCandidate & candidate = m_candidates[i];
        word = candidate.m_display_string;

        Text text (word);

        /* show user candidate as blue. */
        if (CANDIDATE_USER == candidate.m_candidate_type)
            text.appendAttribute (IBUS_ATTR_TYPE_FOREGROUND, 0x000000ef, 0, -1);

        m_lookup_table.appendCandidate (text);
    }

    return TRUE;
}

void
PhoneticEditor::pageUp (void)
{
    if (G_LIKELY (m_lookup_table.pageUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
PhoneticEditor::pageDown (void)
{
    if (G_LIKELY(m_lookup_table.pageDown ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
PhoneticEditor::cursorUp (void)
{
    if (G_LIKELY (m_lookup_table.cursorUp ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
PhoneticEditor::cursorDown (void)
{
    if (G_LIKELY (m_lookup_table.cursorDown ())) {
        updateLookupTableFast ();
        updatePreeditText ();
        updateAuxiliaryText ();
    }
}

void
PhoneticEditor::candidateClicked (guint index, guint button, guint state)
{
    selectCandidateInPage (index);
}

void
PhoneticEditor::reset (void)
{
    m_pinyin_len = 0;
    m_lookup_table.clear ();

    pinyin_reset (m_instance);

    Editor::reset ();
}

void
PhoneticEditor::update (void)
{
    guint lookup_cursor = getLookupCursor ();
    pinyin_guess_candidates (m_instance, lookup_cursor,
                             m_config.sortOption ());

    updateLookupTable ();
    updatePreeditText ();
    updateAuxiliaryText ();
}

guint
PhoneticEditor::getPinyinCursor ()
{
    /* Translate cursor position to pinyin position. */
    size_t pinyin_cursor = 0;
    pinyin_get_pinyin_offset (m_instance, m_cursor, &pinyin_cursor);

    return pinyin_cursor;
}

guint
PhoneticEditor::getLookupCursor (void)
{
    guint lookup_cursor = getPinyinCursor ();

    /* show candidates when pinyin cursor is at end. */
    if (lookup_cursor == m_text.length ())
        lookup_cursor = 0;
    return lookup_cursor;
}

int
PhoneticEditor::selectCandidateInternal (EnhancedCandidate & candidate)
{
    switch (candidate.m_candidate_type) {
    case CANDIDATE_NBEST_MATCH:
    case CANDIDATE_NORMAL:
    case CANDIDATE_USER:
        return m_libpinyin_candidates.selectCandidate (candidate);

    case CANDIDATE_TRADITIONAL_CHINESE:
        return m_traditional_candidates.selectCandidate (candidate);

#ifdef ENABLE_CLOUD_INPUT_MODE
    case CANDIDATE_CLOUD_INPUT:
        return m_cloud_candidates.selectCandidate (candidate);
#endif

#ifdef IBUS_BUILD_LUA_EXTENSION
    case CANDIDATE_LUA_TRIGGER:
        return m_lua_trigger_candidates.selectCandidate (candidate);

    case CANDIDATE_LUA_CONVERTER:
        return m_lua_converter_candidates.selectCandidate (candidate);
#endif

    case CANDIDATE_EMOJI:
        return m_emoji_candidates.selectCandidate (candidate);

    default:
        assert (FALSE);
    }
}

gboolean
PhoneticEditor::removeCandidateInternal (EnhancedCandidate & candidate)
{
    switch (candidate.m_candidate_type) {
    case CANDIDATE_NBEST_MATCH:
    case CANDIDATE_NORMAL:
    case CANDIDATE_USER:
        return m_libpinyin_candidates.removeCandidate (candidate);

    case CANDIDATE_TRADITIONAL_CHINESE:
        return m_traditional_candidates.removeCandidate (candidate);

#ifdef IBUS_BUILD_LUA_EXTENSION
    case CANDIDATE_LUA_TRIGGER:
        return m_lua_trigger_candidates.removeCandidate (candidate);

    case CANDIDATE_LUA_CONVERTER:
        return m_lua_converter_candidates.removeCandidate (candidate);
#endif

    case CANDIDATE_EMOJI:
        return m_emoji_candidates.removeCandidate (candidate);

    default:
        assert (FALSE);
    }
}

#if 0
gboolean
PhoneticEditor::selectCandidate (guint i)
{
    guint len = 0;
    pinyin_get_n_candidate (m_instance, &len);

    if (G_UNLIKELY (i >= len))
        return FALSE;

    guint lookup_cursor = getLookupCursor ();

    lookup_candidate_t * candidate = NULL;
    pinyin_get_candidate (m_instance, i, &candidate);

    lookup_candidate_type_t type;
    pinyin_get_candidate_type (m_instance, candidate, &type);

    if (NBEST_MATCH_CANDIDATE == type) {
        /* as nbest match candidate starts from the beginning of user input. */
        pinyin_choose_candidate (m_instance, 0, candidate);
        guint8 index = 0;
        pinyin_get_candidate_nbest_index(m_instance, candidate, &index);
        commit (index);
        return TRUE;
    }

    lookup_cursor = pinyin_choose_candidate
        (m_instance, lookup_cursor, candidate);

    pinyin_guess_sentence (m_instance);

    if (lookup_cursor == m_text.length ()) {
        commit ();
        return TRUE;
    }

    PinyinKeyPos *pos = NULL;
    pinyin_get_pinyin_key_rest (m_instance, lookup_cursor, &pos);

    guint16 begin = 0;
    pinyin_get_pinyin_key_rest_positions (m_instance, pos, &begin, NULL);
    m_cursor = begin;

    update ();
    return TRUE;
}
#endif

gboolean
PhoneticEditor::selectCandidate (guint index)
{
    if (G_UNLIKELY (index >= m_candidates.size ()))
        return FALSE;

    EnhancedCandidate & candidate = m_candidates[index];
    int action = selectCandidateInternal (candidate);

    if (action & SELECT_CANDIDATE_COMMIT)
        commit (candidate.m_display_string.c_str ());

    if (action & SELECT_CANDIDATE_UPDATE)
        update ();

    return TRUE;
}

gboolean
PhoneticEditor::selectCandidateInPage (guint i)
{
    guint page_size = m_lookup_table.pageSize ();
    guint cursor_pos = m_lookup_table.cursorPos ();

    if (G_UNLIKELY (i >= page_size))
        return FALSE;
    i += (cursor_pos / page_size) * page_size;

    return selectCandidate (i);
}

gboolean
PhoneticEditor::removeCharBefore (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    m_cursor --;
    m_text.erase (m_cursor, 1);

    updatePinyin ();
    update ();

    return TRUE;
}

gboolean
PhoneticEditor::removeCharAfter (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_text.erase (m_cursor, 1);

    updatePinyin ();
    update ();

    return TRUE;
}

gboolean
PhoneticEditor::moveCursorLeft (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    m_cursor --;
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::moveCursorRight (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_cursor ++;
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::moveCursorToBegin (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    m_cursor = 0;
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::moveCursorToEnd (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    m_cursor = m_text.length ();
    update ();
    return TRUE;
}


/* move cursor by word functions */

guint
PhoneticEditor::getCursorLeftByWord (void)
{
    size_t offset = 0;

    pinyin_get_pinyin_offset (m_instance, m_cursor, &offset);

    size_t cursor = 0;

    pinyin_get_left_pinyin_offset(m_instance, offset, &cursor);

    return cursor;
}

guint
PhoneticEditor::getCursorRightByWord (void)
{
    size_t offset = 0;

    pinyin_get_pinyin_offset (m_instance, m_cursor, &offset);

    size_t cursor = 0;

    pinyin_get_right_pinyin_offset(m_instance, offset, &cursor);

    return cursor;
}

gboolean
PhoneticEditor::removeWordBefore (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    guint cursor = getCursorLeftByWord ();
    m_text.erase (cursor, m_cursor - cursor);
    m_cursor = cursor;
    updatePinyin ();
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::removeWordAfter (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    guint cursor = getCursorRightByWord ();
    m_text.erase (m_cursor, cursor - m_cursor);
    updatePinyin ();
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::moveCursorLeftByWord (void)
{
    if (G_UNLIKELY (m_cursor == 0))
        return FALSE;

    guint cursor = getCursorLeftByWord ();

    m_cursor = cursor;
    update ();
    return TRUE;
}

gboolean
PhoneticEditor::moveCursorRightByWord (void)
{
    if (G_UNLIKELY (m_cursor == m_text.length ()))
        return FALSE;

    guint cursor = getCursorRightByWord ();

    m_cursor = cursor;
    update ();
    return TRUE;
}
