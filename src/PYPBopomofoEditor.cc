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

#include "PYPBopomofoEditor.h"
#include "PYConfig.h"
#include "PYLibPinyin.h"
#include "PYPinyinProperties.h"
#include "PYSimpTradConverter.h"
#include "PYHalfFullConverter.h"


using namespace PY;

const static gchar * bopomofo_select_keys[] = {
    "1234567890",
    "asdfghjkl;",
    "1qaz2wsxed",
    "asdfzxcvgb",
    "1234qweras",
    "aoeu;qjkix",
    "aoeuhtnsid",
    "aoeuidhtns",
    "qweasdzxcr"
};

BopomofoEditor::BopomofoEditor
(PinyinProperties & props, Config & config)
    : PhoneticEditor (props, config),
      m_select_mode (FALSE)
{
    m_instance = LibPinyinBackEnd::instance ().allocChewingInstance ();

#ifdef ENABLE_CLOUD_INPUT_MODE
    m_cloud_candidates.setInputMode (Bopomofo);
#endif
}

BopomofoEditor::~BopomofoEditor (void)
{
    LibPinyinBackEnd::instance ().freeChewingInstance (m_instance);
    m_instance = NULL;
}

void
BopomofoEditor::reset (void)
{
    m_select_mode = FALSE;
    PhoneticEditor::reset ();
}

gboolean
BopomofoEditor::insert (gint ch)
{
    /* is full */
    if (G_UNLIKELY (m_text.length () >= MAX_PINYIN_LEN))
        return TRUE;

    m_text.insert (m_cursor++, ch);
    updatePinyin ();
    update ();

    return TRUE;
}


gboolean
BopomofoEditor::processGuideKey (guint keyval, guint keycode,
                                 guint modifiers)
{
    if (!m_config.guideKey ())
        return FALSE;

    if (G_UNLIKELY (cmshm_filter (modifiers) != 0))
        return FALSE;

    if (G_LIKELY (m_select_mode))
        return FALSE;

    if (G_UNLIKELY (keyval == IBUS_space)) {
        m_select_mode = TRUE;
        update ();
        return TRUE;
    }

    return FALSE;
}

gboolean
BopomofoEditor::processAuxiliarySelectKey
(guint keyval, guint keycode, guint modifiers)
{
    if (G_UNLIKELY (cmshm_filter (modifiers) != 0))
        return FALSE;

    guint i;

    switch (keyval) {
    case IBUS_KP_0:
        i = 9;
        if (!m_config.auxiliarySelectKeyKP ())
            return FALSE;
        break;
    case IBUS_KP_1 ... IBUS_KP_9:
        i = keyval - IBUS_KP_1;
        if (!m_config.auxiliarySelectKeyKP ())
            return FALSE;
        break;
    case IBUS_F1 ... IBUS_F10:
        i = keyval - IBUS_F1;
        if (!m_config.auxiliarySelectKeyF ())
            return FALSE;
        break;
    default:
        return FALSE;
    }

    m_select_mode = TRUE;
    selectCandidateInPage (i);

    update ();
    return TRUE;
}

gboolean
BopomofoEditor::processSelectKey (guint keyval, guint keycode,
                                  guint modifiers)
{
    if (G_UNLIKELY (!m_text))
        return FALSE;

    if (G_LIKELY (!m_select_mode && ((modifiers & IBUS_MOD1_MASK) == 0)))
        return FALSE;

    const gchar * pos = NULL;
    const gchar * keys = bopomofo_select_keys[m_config.selectKeys ()];
    for ( const gchar * p = keys; *p; ++p ) {
        if ( *p == keyval )
            pos = p;
    }

    if (pos == NULL)
        return FALSE;

    m_select_mode = TRUE;

    guint i = pos - keys;
    selectCandidateInPage (i);

    update ();
    return TRUE;
}

gboolean
BopomofoEditor::processBopomofo (guint keyval, guint keycode,
                                 guint modifiers)
{
    if (G_UNLIKELY (cmshm_filter (modifiers) != 0))
        return m_text ? TRUE : FALSE;

    gchar ** symbols = NULL;
    if (!pinyin_in_chewing_keyboard (m_instance, keyval, &symbols))
        return FALSE;
    g_strfreev (symbols);

    if (keyval == IBUS_space)
        return FALSE;

    m_select_mode = FALSE;

    return insert (keyval);
}

gboolean
BopomofoEditor::processKeyEvent (guint keyval, guint keycode,
                                          guint modifiers)
{
    modifiers &= (IBUS_SHIFT_MASK |
                  IBUS_CONTROL_MASK |
                  IBUS_MOD1_MASK |
                  IBUS_SUPER_MASK |
                  IBUS_HYPER_MASK |
                  IBUS_META_MASK |
                  IBUS_LOCK_MASK);

    if (G_UNLIKELY (processGuideKey (keyval, keycode, modifiers)))
        return TRUE;
    if (G_UNLIKELY (processSelectKey (keyval, keycode, modifiers)))
        return TRUE;
    if (G_UNLIKELY (processAuxiliarySelectKey (keyval, keycode,
                                               modifiers)))
        return TRUE;
    if (G_LIKELY (processBopomofo (keyval, keycode, modifiers)))
        return TRUE;

    switch (keyval) {
    case IBUS_space:
        m_select_mode = TRUE;
        return processSpace (keyval, keycode, modifiers);

    case IBUS_Return:
    case IBUS_KP_Enter:
        /* no user input */
        if (m_text.empty ())
            return FALSE;

        if (m_config.enterKey ())
            commit ();
        else {
            Text text (m_text.c_str ());
            commitText (text);
        }

        reset ();
        return TRUE;

    case IBUS_Up:        case IBUS_KP_Up:
    case IBUS_Down:      case IBUS_KP_Down:
    case IBUS_Page_Up:   case IBUS_KP_Page_Up:
    case IBUS_Page_Down: case IBUS_KP_Page_Down:
    case IBUS_Tab:
        m_select_mode = TRUE;
        return PhoneticEditor::processFunctionKey
            (keyval, keycode, modifiers);

    case IBUS_BackSpace:
    case IBUS_Delete:    case IBUS_KP_Delete:
    case IBUS_Left:      case IBUS_KP_Left:
    case IBUS_Right:     case IBUS_KP_Right:
    case IBUS_Home:      case IBUS_KP_Home:
    case IBUS_End:       case IBUS_KP_End:
        m_select_mode = FALSE;
        return PhoneticEditor::processFunctionKey
            (keyval, keycode, modifiers);

    default:
        return PhoneticEditor::processFunctionKey
            (keyval, keycode, modifiers);
    }
    return FALSE;
}

void
BopomofoEditor::updateLookupTableLabel (void)
{
    String labels = bopomofo_select_keys[m_config.selectKeys ()];

    size_t len = MIN (labels.length (), m_config.pageSize ());
    for (size_t i = 0; i < len; ++i) {
        String label = (gchar) labels[i];
        Text text (label);
        m_lookup_table.setLabel (i, text);
    }
}

void
BopomofoEditor::updateLookupTable (void)
{
    // needed by updatePreeditText
    updateCandidates ();

    if (!m_select_mode) {
        hideLookupTable ();
        return;
    }

    m_lookup_table.clear ();

    fillLookupTable ();
    updateLookupTableLabel ();
    if (m_lookup_table.size()) {
        Editor::updateLookupTable (m_lookup_table, TRUE);
    } else {
        hideLookupTable ();
    }
}

void
BopomofoEditor::updatePinyin (void)
{
    if (G_UNLIKELY (m_text.empty ())) {
        m_pinyin_len = 0;
        /* TODO: check whether to replace "" with NULL. */
        pinyin_parse_more_chewings (m_instance, "");
        pinyin_guess_sentence (m_instance);
        return;
    }

    m_pinyin_len =
        pinyin_parse_more_chewings (m_instance, m_text.c_str ());
    pinyin_guess_sentence (m_instance);
}

void
BopomofoEditor::commit (const gchar *str)
{
    if (G_UNLIKELY (m_text.empty ()))
        return;

    m_buffer.clear ();

    /* sentence candidate */
    m_buffer << str;

    /* text after pinyin */
    const gchar *p = m_text.c_str() + m_pinyin_len;
    while (*p != '\0') {
        gchar ** symbols = NULL;
        if (pinyin_in_chewing_keyboard (m_instance, *p, &symbols)) {
            g_assert (1 == g_strv_length (symbols));
            m_buffer << symbols[0];
            g_strfreev (symbols);
        } else {
            if (G_UNLIKELY (m_props.modeFull ())) {
                m_buffer.appendUnichar (HalfFullConverter::toFull (*p));
            } else {
                m_buffer << *p;
            }
        }
        ++p;
    }

    Text text (m_buffer.c_str ());
    commitText (text);

    reset();
}

void
BopomofoEditor::updatePreeditText ()
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

    /* text after pinyin */
    const gchar *p = m_text.c_str() + m_pinyin_len;
    while (*p != '\0') {
        gchar ** symbols = NULL;
        if (pinyin_in_chewing_keyboard (m_instance, *p, &symbols)) {
            g_assert (1 == g_strv_length (symbols));
            m_buffer << symbols[0];
            g_strfreev (symbols);
        } else {
            if (G_UNLIKELY (m_props.modeFull ())) {
                m_buffer.appendUnichar (HalfFullConverter::toFull (*p));
            } else {
                m_buffer << *p;
            }
        }
        ++p;
    }

    StaticText preedit_text (m_buffer);
    /* underline */
    preedit_text.appendAttribute (IBUS_ATTR_TYPE_UNDERLINE, IBUS_ATTR_UNDERLINE_SINGLE, 0, -1);

    size_t offset = 0;
    guint cursor = getPinyinCursor ();
    pinyin_get_character_offset(m_instance, sentence, cursor, &offset);
    Editor::updatePreeditText (preedit_text, offset, TRUE);

    g_free (sentence);
}

void
BopomofoEditor::updateAuxiliaryText (void)
{
    if (G_UNLIKELY (m_text.empty ())) {
        if (DISPLAY_STYLE_TRADITIONAL == m_config.displayStyle ())
            hideAuxiliaryText ();
        if (DISPLAY_STYLE_COMPACT == m_config.displayStyle ())
            hidePreeditText ();
        return;
    }

    m_buffer.clear ();

    gchar * aux_text = NULL;
    pinyin_get_chewing_auxiliary_text (m_instance, m_cursor, &aux_text);
    m_buffer << aux_text;
    g_free(aux_text);

    /* append rest text */
    const gchar * p = m_text.c_str() + m_pinyin_len;
    m_buffer << p;

    StaticText text (m_buffer);
    if (DISPLAY_STYLE_TRADITIONAL == m_config.displayStyle ())
        Editor::updateAuxiliaryText (text, TRUE);
    if (DISPLAY_STYLE_COMPACT == m_config.displayStyle ())
        Editor::updatePreeditText (text, 0, TRUE);
}

