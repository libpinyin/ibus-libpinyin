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

#include "PYPFullPinyinEditor.h"
#include "PYConfig.h"
#include "PYLibPinyin.h"

using namespace PY;

FullPinyinEditor::FullPinyinEditor
(PinyinProperties & props, Config & config)
    : PinyinEditor (props, config)
{
    m_instance = LibPinyinBackEnd::instance ().allocPinyinInstance ();
}

FullPinyinEditor::~FullPinyinEditor (void)
{
    LibPinyinBackEnd::instance ().freePinyinInstance (m_instance);
    m_instance = NULL;
}

void
FullPinyinEditor::reset (void)
{
    PinyinEditor::reset ();
}

gboolean
FullPinyinEditor::insert (gint ch)
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
FullPinyinEditor::processKeyEvent (guint keyval,
                                            guint keycode,
                                            guint modifiers)
{
    return PinyinEditor::processKeyEvent (keyval, keycode, modifiers);
}

void
FullPinyinEditor::updatePinyin (void)
{
    if (G_UNLIKELY (m_text.empty ())) {
        m_pinyin_len = 0;
        /* TODO: check whether to replace "" with NULL. */
        pinyin_parse_more_full_pinyins (m_instance, "");
        pinyin_guess_sentence (m_instance);
        return;
    }

    m_pinyin_len =
        pinyin_parse_more_full_pinyins (m_instance, m_text.c_str ());
    pinyin_guess_sentence (m_instance);
}

void
FullPinyinEditor::updateAuxiliaryText (void)
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
    pinyin_get_full_pinyin_auxiliary_text (m_instance, m_cursor, &aux_text);
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

guint
FullPinyinEditor::getLookupCursor (void)
{
    guint lookup_cursor = getPinyinCursor ();

    /* as pinyin_get_pinyin_offset can't handle the last "'" characters,
       strip the string to work around it here. */
    String stripped = m_text;
    size_t pos = stripped.find_last_not_of ("'") + 1;
    if (pos < stripped.length ())
        stripped.erase (pos);

    /* show candidates when pinyin cursor is at end. */
    if (lookup_cursor == stripped.length ())
        lookup_cursor = 0;
    return lookup_cursor;
}
