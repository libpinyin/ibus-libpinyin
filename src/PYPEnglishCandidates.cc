/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2021 Peng Wu <alexepico@gmail.com>
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

#include "PYPEnglishCandidates.h"
#include <algorithm>
#include <assert.h>

using namespace PY;

static const int MINIMAL_ENGLISH_CHARACTERS = 4;
static const int MAXIMAL_ENGLISH_CANDIDATES = 2;

EnglishCandidates::EnglishCandidates (Editor *editor)
    : m_train_factor (0.1)
{
    m_editor = editor;

    m_english_database = & EnglishDatabase::instance ();
}

static bool compare_string_length (const std::string & lhs, const std::string &rhs)
{
    return lhs.length () < rhs.length ();
}

gboolean
EnglishCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    if (m_editor->m_text.length () < MINIMAL_ENGLISH_CHARACTERS)
        return FALSE;

    const char *prefix = m_editor->m_text.c_str ();
    std::vector<std::string> words;

    // Find the insertion position
    size_t insert_index = 0;
    for (auto pos = candidates.begin (); pos != candidates.end (); ++pos, ++insert_index) {
        if (CANDIDATE_NBEST_MATCH != pos->m_candidate_type &&
            CANDIDATE_LONGER != pos->m_candidate_type &&
            CANDIDATE_LONGER_USER != pos->m_candidate_type)
            break;
    }

    EnhancedCandidate enhanced;
    enhanced.m_candidate_type = CANDIDATE_ENGLISH;

    int count = 0;
    if (m_english_database->listWords (prefix, words)) {
        // sort the words by length and frequency
        std::stable_sort (words.begin (), words.end (), compare_string_length);

        // list the shortest words here
        for (auto iter = words.begin (); iter != words.end (); ++iter) {
            if (count >= MAXIMAL_ENGLISH_CANDIDATES)
                break;

            enhanced.m_candidate_id = count;
            enhanced.m_display_string = *iter;

            // Use an integer index to avoid iterator UB
            candidates.insert(candidates.begin() + insert_index + count, enhanced);

            ++count;
        }

        return TRUE;
    }

    return FALSE;
}

int
EnglishCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_ENGLISH == enhanced.m_candidate_type);
    assert (enhanced.m_candidate_id < MAXIMAL_ENGLISH_CANDIDATES);

    m_english_database->train (enhanced.m_display_string.c_str (), m_train_factor);

    return SELECT_CANDIDATE_DIRECT_COMMIT;
}

gboolean
EnglishCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_ENGLISH == enhanced.m_candidate_type);
    assert (enhanced.m_candidate_id < MAXIMAL_ENGLISH_CANDIDATES);

    return m_english_database->deleteUserWord (enhanced.m_display_string.c_str ());
}
