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

#include "PYPPinyinEditor.h"
#include "PYConfig.h"
#include "PYPinyinProperties.h"
#include "PYSimpTradConverter.h"
#include "PYHalfFullConverter.h"
#include "PYLibPinyin.h"

using namespace PY;

/* init static members*/
PinyinEditor::PinyinEditor (PinyinProperties & props,
                                              Config & config)
    : PhoneticEditor (props, config)
{
}


/**
 * process pinyin
 */
inline gboolean
PinyinEditor::processPinyin (guint keyval, guint keycode,
                                      guint modifiers)
{
    if (G_UNLIKELY (cmshm_filter (modifiers) != 0))
        return m_text ? TRUE : FALSE;

    return insert (keyval);
}

/**
 * process numbers
 */
inline gboolean
PinyinEditor::processNumber (guint keyval, guint keycode,
                                      guint modifiers)
{
    guint i;

    if (m_text.empty ())
        return FALSE;

    switch (keyval) {
    case IBUS_0:
    case IBUS_KP_0:
        i = 9;
        break;
    case IBUS_1 ... IBUS_9:
        i = keyval - IBUS_1;
        break;
    case IBUS_KP_1 ... IBUS_KP_9:
        i = keyval - IBUS_KP_1;
        break;
    default:
        g_return_val_if_reached (FALSE);
    }

    if (modifiers == 0)
        selectCandidateInPage (i);

    update ();
    return TRUE;
}

inline gboolean
PinyinEditor::processPunct (guint keyval, guint keycode,
                                     guint modifiers)
{
    if (m_text.empty ())
        return FALSE;

    if (cmshm_filter (modifiers) != 0)
        return TRUE;

    switch (keyval) {
    case IBUS_apostrophe:
        return insert (keyval);
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
    }

    if (m_config.autoCommit ()) {
        if (m_lookup_table.size ()) {
            selectCandidate (m_lookup_table.cursorPos ());
        }
        commit ();
        return FALSE;
    }

    return FALSE;
}

inline gboolean
PinyinEditor::processFunctionKey (guint keyval, guint keycode,
                                           guint modifiers)
{
    if (m_text.empty ())
        return FALSE;

    /* ignore numlock */
    modifiers = cmshm_filter (modifiers);

    if (modifiers != 0 && modifiers != IBUS_CONTROL_MASK)
        return TRUE;

    /* process some cursor control keys */
    if (modifiers == 0) { /* no modifiers. */
        switch (keyval) {
        case IBUS_Shift_L:
            if (!m_config.shiftSelectCandidate ())
                return FALSE;
            selectCandidateInPage (1);
            return TRUE;

        case IBUS_Shift_R:
            if (!m_config.shiftSelectCandidate ())
                return FALSE;
            selectCandidateInPage (2);
            return TRUE;
        }
    }

    return PhoneticEditor::processFunctionKey (keyval, keycode,
                                                        modifiers);
}

gboolean
PinyinEditor::processKeyEvent (guint keyval, guint keycode,
                                        guint modifiers)
{
    modifiers &= (IBUS_SHIFT_MASK |
                  IBUS_CONTROL_MASK |
                  IBUS_MOD1_MASK |
                  IBUS_SUPER_MASK |
                  IBUS_HYPER_MASK |
                  IBUS_META_MASK |
                  IBUS_LOCK_MASK);

    switch (keyval) {
    /* letters */
    case IBUS_a ... IBUS_z:
        return processPinyin (keyval, keycode, modifiers);
    case IBUS_0 ... IBUS_9:
    case IBUS_KP_0 ... IBUS_KP_9:
        return processNumber (keyval, keycode, modifiers);
    case IBUS_exclam ... IBUS_slash:
    case IBUS_colon ... IBUS_at:
    case IBUS_bracketleft ... IBUS_quoteleft:
    case IBUS_braceleft ... IBUS_asciitilde:
        return processPunct (keyval, keycode, modifiers);
    case IBUS_space:
        return processSpace (keyval, keycode, modifiers);
    default:
        return processFunctionKey (keyval, keycode, modifiers);
    }
}

void
PinyinEditor::commit (const gchar *str)
{
    if (G_UNLIKELY (m_text.empty ()))
        return;

    m_buffer.clear ();

    /* sentence candidate */
    m_buffer << str;

    /* text after pinyin */
    const gchar *p = m_text.c_str() + m_pinyin_len;
    if (G_UNLIKELY (m_props.modeFull ())) {
        while (*p != '\0') {
            m_buffer.appendUnichar (HalfFullConverter::toFull (*p++));
        }
    } else {
        m_buffer << p;
    }

    Text text (m_buffer.c_str ());
    commitText (text);

    reset();
}

void
PinyinEditor::updatePreeditText ()
{
    if (DISPLAY_STYLE_COMPACT == m_config.displayStyle ())
        return;

    guint num = 0;
    pinyin_get_n_candidate (m_instance, &num);

    /* preedit text = guessed sentence + un-parsed pinyin text */
    if (G_UNLIKELY (m_text.empty () || 0 == num)) {
        hidePreeditText ();
        return;
    }

    m_buffer.clear ();

    /* probe nbest match candidate */
    lookup_candidate_type_t type;
    lookup_candidate_t * candidate = NULL;
    pinyin_get_candidate (m_instance, 0, &candidate);
    pinyin_get_candidate_type (m_instance, candidate, &type);

    gchar * sentence = NULL;
    if (NBEST_MATCH_CANDIDATE == type) {
        pinyin_get_sentence (m_instance, 0, &sentence);
        m_buffer<<m_candidates[0].m_display_string;
    }

    /* append rest text */
    const gchar *p = m_text.c_str () + m_pinyin_len;
    m_buffer << p;

    StaticText preedit_text (m_buffer);
    /* underline */
    preedit_text.appendAttribute (IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, -1);

    size_t offset = 0;
    guint cursor = getPinyinCursor ();
    pinyin_get_character_offset(m_instance, sentence, cursor, &offset);
    Editor::updatePreeditText (preedit_text, offset, TRUE);

    g_free (sentence);
}

#if 0
void
PinyinEditor::updateAuxiliaryText ()
{
    if (G_UNLIKELY (m_text.empty ())) {
        hideAuxiliaryText ();
        return;
    }

    m_buffer.clear ();

    /* Note: cursor handling is defered to full/double pinyin editors. */

    guint len = 0;
    pinyin_get_n_pinyin (m_instance, &len);

    for (guint i = 0; i < len; ++i) {
        if (G_LIKELY (i))
            m_buffer << ' ';

        PinyinKey *key = NULL;
        pinyin_get_pinyin_key (m_instance, i, &key);

        gchar * str = NULL;
        pinyin_get_pinyin_string (m_instance, key, &str);

        m_buffer << str;
        g_free (str);
    }

    /* append rest text */
    const gchar *p = m_text.c_str() + m_pinyin_len;
    m_buffer << p;

    StaticText aux_text (m_buffer);
    Editor::updateAuxiliaryText (aux_text, TRUE);
}
#endif

void
PinyinEditor::updateLookupTable ()
{
    m_lookup_table.setPageSize (m_config.pageSize ());
    m_lookup_table.setOrientation (m_config.orientation ());
    PhoneticEditor::updateLookupTable ();
}

