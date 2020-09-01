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

#include "PYPDoublePinyinEditor.h"
#include "PYConfig.h"
#include "PYLibPinyin.h"

using namespace PY;

/*
 * c in 'a' ... 'z' => id = c - 'a'
 * c == ';'         => id = 26
 * else             => id = -1
 */
#define ID(c) \
    ((c >= IBUS_a && c <= IBUS_z) ? c - IBUS_a : (c == IBUS_semicolon ? 26 : -1))

#define IS_ALPHA(c) \
        ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))


DoublePinyinEditor::DoublePinyinEditor
( PinyinProperties & props, Config & config)
    : PinyinEditor (props, config)
{
    m_instance = LibPinyinBackEnd::instance ().allocPinyinInstance ();

#ifdef ENABLE_CLOUD_INPUT_MODE
    m_cloud_candidates.setInputMode (DoublePinyin);
#endif
}

DoublePinyinEditor::~DoublePinyinEditor (void)
{
    LibPinyinBackEnd::instance ().freePinyinInstance (m_instance);
    m_instance = NULL;
}

gboolean
DoublePinyinEditor::insert (gint ch)
{
    /* is full */
    if (G_UNLIKELY (m_text.length () >= MAX_PINYIN_LEN))
        return TRUE;

    gint id = ID (ch);
    if (id == -1) {
        /* it is not available ch */
        return FALSE;
    }

#if 0
    if (G_UNLIKELY (m_text.empty () && ID_TO_SHENG (id) == PINYIN_ID_VOID)) {
        return FALSE;
    }
#endif

    m_text.insert (m_cursor++, ch);
    updatePinyin ();
    update ();

    return TRUE;
}

void DoublePinyinEditor::reset (void)
{
    PinyinEditor::reset ();
}

gboolean
DoublePinyinEditor::processKeyEvent (guint keyval, guint keycode,
                                              guint modifiers)
{
    /* handle ';' key */
    if (G_UNLIKELY (keyval == IBUS_semicolon)) {
        if (cmshm_filter (modifiers) == 0) {

            if (m_text.empty ())
                return FALSE;

            if (insert (keyval))
                return TRUE;
        }
    }

    return PinyinEditor::processKeyEvent (keyval, keycode, modifiers);
}

void
DoublePinyinEditor::updatePinyin (void)
{
    if (G_UNLIKELY (m_text.empty ())) {
        m_pinyin_len = 0;
        /* TODO: check whether to replace "" with NULL. */
        pinyin_parse_more_double_pinyins (m_instance, "");
        pinyin_guess_sentence(m_instance);
        return;
    }

    m_pinyin_len =
        pinyin_parse_more_double_pinyins (m_instance, m_text.c_str ());
    pinyin_guess_sentence (m_instance);
}


void
DoublePinyinEditor::updateAuxiliaryText (void)
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
    pinyin_get_double_pinyin_auxiliary_text (m_instance, m_cursor, &aux_text);
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
