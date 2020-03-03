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

#include "PYPLibPinyinCandidates.h"
#include <assert.h>
#include <pinyin.h>
#include "PYConfig.h"
#include "PYLibPinyin.h"
#include "PYPPhoneticEditor.h"


using namespace PY;

gboolean
LibPinyinCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    pinyin_instance_t *instance = m_editor->m_instance;

    guint len = 0;
    pinyin_get_n_candidate (instance, &len);

    for (guint i = 0; i < len; i++) {
        lookup_candidate_t * candidate = NULL;
        pinyin_get_candidate (instance, i, &candidate);

        lookup_candidate_type_t type;
        pinyin_get_candidate_type (instance, candidate, &type);

        const gchar * phrase_string = NULL;
        pinyin_get_candidate_string (instance, candidate, &phrase_string);

        EnhancedCandidate enhanced;

        switch (type) {
        case NBEST_MATCH_CANDIDATE:
            enhanced.m_candidate_type = CANDIDATE_NBEST_MATCH;
            break;

        case NORMAL_CANDIDATE:
        case ADDON_CANDIDATE:
            enhanced.m_candidate_type = CANDIDATE_NORMAL;

            if (pinyin_is_user_candidate (instance, candidate))
                enhanced.m_candidate_type = CANDIDATE_USER;

            break;

        default:
            assert (FALSE);
        }

        enhanced.m_candidate_id = i;
        enhanced.m_display_string = phrase_string;

        candidates.push_back (enhanced);
    }

    return TRUE;
}

int
LibPinyinCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    pinyin_instance_t * instance = m_editor->m_instance;
    assert (CANDIDATE_NBEST_MATCH == enhanced.m_candidate_type ||
            CANDIDATE_NORMAL == enhanced.m_candidate_type ||
            CANDIDATE_USER == enhanced.m_candidate_type);

    guint len = 0;
    pinyin_get_n_candidate (instance, &len);

    if (G_UNLIKELY (enhanced.m_candidate_id >= len))
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    guint lookup_cursor = m_editor->getLookupCursor ();

    lookup_candidate_t * candidate = NULL;
    pinyin_get_candidate (instance, enhanced.m_candidate_id, &candidate);

    gchar * str = NULL;
    if (CANDIDATE_NBEST_MATCH == enhanced.m_candidate_type) {
        /* because nbest match candidate
           starts from the beginning of user input. */
        pinyin_choose_candidate (instance, 0, candidate);

        guint8 index = 0;
        pinyin_get_candidate_nbest_index(instance, candidate, &index);

        if (index != 0)
            pinyin_train (instance, index);

        pinyin_get_sentence (instance, index, &str);
        if (m_editor->m_config.rememberEveryInput ())
            LibPinyinBackEnd::instance ().rememberUserInput (instance, str);
        LibPinyinBackEnd::instance ().modified();
        g_free (str);

        return SELECT_CANDIDATE_COMMIT;
    }

    lookup_cursor = pinyin_choose_candidate
        (instance, lookup_cursor, candidate);

    pinyin_guess_sentence (instance);

    if (lookup_cursor == m_editor->m_text.length ()) {
        pinyin_get_sentence (instance, 0, &str);
        enhanced.m_display_string = str;
        pinyin_train (instance, 0);

        if (m_editor->m_config.rememberEveryInput ())
            LibPinyinBackEnd::instance ().rememberUserInput (instance, str);
        LibPinyinBackEnd::instance ().modified ();
        g_free (str);

        return SELECT_CANDIDATE_MODIFY_IN_PLACE|SELECT_CANDIDATE_COMMIT;
    }

    PinyinKeyPos *pos = NULL;
    pinyin_get_pinyin_key_rest (instance, lookup_cursor, &pos);

    guint16 begin = 0;
    pinyin_get_pinyin_key_rest_positions (instance, pos, &begin, NULL);
    m_editor->m_cursor = begin;

    return SELECT_CANDIDATE_UPDATE;
}

gboolean
LibPinyinCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    pinyin_instance_t * instance = m_editor->m_instance;

    if (enhanced.m_candidate_type != CANDIDATE_USER)
        return FALSE;

    lookup_candidate_t * candidate = NULL;
    guint index = enhanced.m_candidate_id;
    pinyin_get_candidate (instance, index, &candidate);
    assert (pinyin_is_user_candidate (instance, candidate));
    pinyin_remove_user_candidate (instance, candidate);

    return TRUE;
}
