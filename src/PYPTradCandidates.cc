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

#include "PYPTradCandidates.h"
#include <assert.h>
#include "PYString.h"
#include "PYPPhoneticEditor.h"

using namespace PY;

gboolean
TraditionalCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    m_candidates.clear ();

    String trad;
    for (guint i = 0; i < candidates.size (); i++) {
        EnhancedCandidate & enhanced = candidates[i];

        m_candidates.push_back (enhanced);

        enhanced.m_candidate_type = CANDIDATE_TRADITIONAL_CHINESE;
        enhanced.m_candidate_id = i;

        trad.truncate (0);
        m_converter.simpToTrad (enhanced.m_display_string.c_str (), trad);
        enhanced.m_display_string = trad;
    }

    return TRUE;
}

int
TraditionalCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    guint id = enhanced.m_candidate_id;
    assert (CANDIDATE_TRADITIONAL_CHINESE == enhanced.m_candidate_type);

    if (G_UNLIKELY (id >= m_candidates.size ()))
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    int action = m_editor->selectCandidateInternal (m_candidates[id]);

    if (action & SELECT_CANDIDATE_MODIFY_IN_PLACE) {
        String trad;
        m_converter.simpToTrad
            (m_candidates[id].m_display_string.c_str (), trad);
        enhanced.m_display_string = trad;
    }

    return action;
}

gboolean
TraditionalCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    guint id = enhanced.m_candidate_id;
    assert (CANDIDATE_TRADITIONAL_CHINESE == enhanced.m_candidate_type);

    if (G_UNLIKELY (id >= m_candidates.size ()))
        return FALSE;

    return m_editor->removeCandidateInternal (m_candidates[id]);
}
