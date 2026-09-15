/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2018 Peng Wu <alexepico@gmail.com>
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

#include "PYPSuggestionCandidates.h"
#include <assert.h>
#include <pinyin.h>
#include "PYPSuggestionEditor.h"

using namespace PY;

static gboolean
has_visible_character (const gchar *text)
{
    if (text == NULL || !g_utf8_validate (text, -1, NULL))
        return FALSE;

    for (const gchar *p = text; *p != '\0'; p = g_utf8_next_char (p)) {
        if (g_unichar_isgraph (g_utf8_get_char (p)))
            return TRUE;
    }

    return FALSE;
}

gboolean
SuggestionCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    pinyin_instance_t *instance = m_editor->m_instance;

    guint len = 0;
    pinyin_get_n_candidate (instance, &len);

    for (guint i = 0; i < len; i++) {
        lookup_candidate_t * candidate = NULL;
        pinyin_get_candidate (instance, i, &candidate);

        lookup_candidate_type_t type;
        pinyin_get_candidate_type (instance, candidate, &type);
        CandidateType candidate_type;
        switch (type) {
        case PREDICTED_BIGRAM_CANDIDATE:
            candidate_type = CANDIDATE_PREDICTED_BIGRAM;
            break;
        case PREDICTED_PREFIX_CANDIDATE:
            candidate_type = CANDIDATE_PREDICTED_PREFIX;
            break;
        case PREDICTED_PUNCTUATION_CANDIDATE:
            candidate_type = CANDIDATE_PREDICTED_PUNCTUATION;
            break;
        default:
            assert(FALSE);
        }

        const gchar * phrase_string = NULL;
        pinyin_get_candidate_string (instance, candidate, &phrase_string);
        if (!has_visible_character (phrase_string))
            continue;

        EnhancedCandidate enhanced;
        enhanced.m_candidate_type = candidate_type;
        enhanced.m_candidate_id = i;
        enhanced.m_display_string = phrase_string;

        candidates.push_back (enhanced);
    }

    return TRUE;
}

int
SuggestionCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    pinyin_instance_t * instance = m_editor->m_instance;
    assert (CANDIDATE_PREDICTED_BIGRAM == enhanced.m_candidate_type ||
            CANDIDATE_PREDICTED_PREFIX == enhanced.m_candidate_type ||
            CANDIDATE_PREDICTED_PUNCTUATION == enhanced.m_candidate_type);

    guint len = 0;
    pinyin_get_n_candidate (instance, &len);

    if (G_UNLIKELY (enhanced.m_candidate_id >= len))
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    lookup_candidate_t * candidate = NULL;
    pinyin_get_candidate (instance, enhanced.m_candidate_id, &candidate);
    pinyin_choose_predicted_candidate (instance, candidate);

    return SELECT_CANDIDATE_COMMIT;
}
