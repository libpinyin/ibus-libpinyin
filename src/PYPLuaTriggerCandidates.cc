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

#include "PYPLuaTriggerCandidates.h"
#include <assert.h>
#include "PYString.h"
#include "PYConfig.h"
#include "PYPPhoneticEditor.h"

using namespace PY;

LuaTriggerCandidates::LuaTriggerCandidates (Editor *editor)
{
    m_editor = editor;
}

gboolean
LuaTriggerCandidates::setLuaPlugin (IBusEnginePlugin *plugin)
{
    m_lua_plugin = plugin;
    return TRUE;
}

gboolean
LuaTriggerCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    if (!m_lua_plugin)
        return FALSE;

    EnhancedCandidate enhanced;
    enhanced.m_candidate_type = CANDIDATE_LUA_TRIGGER;
    enhanced.m_candidate_id = 0;

    std::vector<EnhancedCandidate>::iterator pos;
    for (pos = candidates.begin (); pos != candidates.end (); ++pos) {
        if (CANDIDATE_NBEST_MATCH != pos->m_candidate_type)
            break;
    }

    const char * lua_function_name = NULL;
    const char * text = m_editor->m_text;
    gchar * string = NULL;

    if (ibus_engine_plugin_match_input
        (m_lua_plugin, text, &lua_function_name)) {
        ibus_engine_plugin_call (m_lua_plugin, lua_function_name, text);

        string = ibus_engine_plugin_get_first_result (m_lua_plugin);
        enhanced.m_display_string = string;
        g_free (string);

        candidates.insert (pos, enhanced);
        return TRUE;
    } else {
        int num = std::min
            (m_editor->m_config.pageSize (), (guint)candidates.size ());
        for (int i = 0; i < num; ++i) {
            text = candidates[i].m_display_string.c_str ();
            if (ibus_engine_plugin_match_candidate
                (m_lua_plugin, text, &lua_function_name)) {
                ibus_engine_plugin_call (m_lua_plugin, lua_function_name, text);

                string = ibus_engine_plugin_get_first_result (m_lua_plugin);
                enhanced.m_display_string = string;
                g_free (string);

                candidates.insert (pos, enhanced);
                return TRUE;
            }
        }
    }

    return FALSE;
}

int
LuaTriggerCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_LUA_TRIGGER == enhanced.m_candidate_type);
    assert (0 == enhanced.m_candidate_id);

    return SELECT_CANDIDATE_COMMIT;
}

gboolean
LuaTriggerCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_LUA_TRIGGER == enhanced.m_candidate_type);
    assert (0 == enhanced.m_candidate_id);

    return FALSE;
}
