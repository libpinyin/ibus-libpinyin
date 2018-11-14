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


#include "PYPEmojiCandidates.h"
#include <assert.h>
#include <algorithm>
#include <cstring>
#include "PYPPhoneticEditor.h"
#include "PYConfig.h"
#include "PYPEmojiTable.h"

using namespace PY;

EmojiCandidates::EmojiCandidates (Editor *editor)
{
    m_editor = editor;
}

static bool compare_match_less_than (const EmojiItem & lhs,
                                     const EmojiItem & rhs) {
    return 0 > std::strcmp (lhs.m_emoji_match, rhs.m_emoji_match);
}

static bool search_emoji (const EmojiItem * emojis,
                          guint emojis_len,
                          const char * match,
                          std::string & emoji) {
    const EmojiItem item = {match, NULL};

    std::pair<const EmojiItem *, const EmojiItem *> range;
    range = std::equal_range (emojis, emojis + emojis_len,
                              item, compare_match_less_than);

    guint range_len = range.second - range.first;
    assert(range_len <= 1);

    if (range_len == 1) {
        const EmojiItem * index = range.first;

        emoji = index->m_emoji_string;
        return true;
    }

    return false;
}

gboolean
EmojiCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    EnhancedCandidate enhanced;
    enhanced.m_candidate_type = CANDIDATE_EMOJI;
    enhanced.m_candidate_id = 0;

    std::vector<EnhancedCandidate>::iterator pos;
    for (pos = candidates.begin (); pos != candidates.end (); ++pos) {
        if (CANDIDATE_NBEST_MATCH != pos->m_candidate_type)
            break;
    }

    std::string emoji;
    if (search_emoji (english_emoji_table,
                      G_N_ELEMENTS (english_emoji_table),
                      m_editor->m_text, emoji)) {
        enhanced.m_display_string = emoji;
        candidates.insert (pos, enhanced);
        return TRUE;
    } else {
        int num = std::min
            (m_editor->m_config.pageSize (), (guint)candidates.size ());
        for (int i = 0; i < num; ++i) {
            String text = candidates[i].m_display_string;

            if (search_emoji (chinese_emoji_table,
                              G_N_ELEMENTS (chinese_emoji_table),
                              text, emoji)) {

                enhanced.m_display_string = emoji;
                candidates.insert (pos, enhanced);
                return TRUE;
            }
        }
    }

    return FALSE;
}

int
EmojiCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_EMOJI == enhanced.m_candidate_type);
    assert (0 == enhanced.m_candidate_id);

    return SELECT_CANDIDATE_COMMIT;
}
