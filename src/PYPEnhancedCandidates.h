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

#ifndef __PY_LIB_PINYIN_ENHANCED_CANDIDATES_H_
#define __PY_LIB_PINYIN_ENHANCED_CANDIDATES_H_

#include <glib.h>
#include <string>
#include <vector>

namespace PY {

enum CandidateType {
    CANDIDATE_NBEST_MATCH = 1,
    /* not included with user candidate */
    CANDIDATE_NORMAL,
    /* both normal candidate and user candidate */
    CANDIDATE_USER,
    CANDIDATE_TRADITIONAL_CHINESE,
    CANDIDATE_LUA_TRIGGER,
    CANDIDATE_LUA_CONVERTER,
    CANDIDATE_SUGGESTION,
    CANDIDATE_CLOUD_INPUT,
    CANDIDATE_EMOJI
};

enum SelectCandidateAction {
    SELECT_CANDIDATE_ALREADY_HANDLED = 0x0,
    /* commit the text without change. */
    SELECT_CANDIDATE_COMMIT = 0x1,
    /* modify the current candidate in place */
    SELECT_CANDIDATE_MODIFY_IN_PLACE = 0x2,
    /* need to call update method in class Editor. */
    SELECT_CANDIDATE_UPDATE = 0x4
};

struct EnhancedCandidate {
    CandidateType m_candidate_type;
    guint m_candidate_id;
    std::string m_display_string;
};

template <class IEditor>
class EnhancedCandidates {

public:
    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);

    int selectCandidate (EnhancedCandidate & enhanced);
    gboolean removeCandidate (EnhancedCandidate & enhanced);

protected:

    /* will call selectCandidateInternal method of class IEditor. */
    IEditor *m_editor;
};

};

#endif
