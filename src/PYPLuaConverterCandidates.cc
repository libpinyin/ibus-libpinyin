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

#include "PYPLuaConverterCandidates.h"
#include <assert.h>
#include "PYString.h"
#include "PYConfig.h"
#include "PYPPhoneticEditor.h"

using namespace PY;

LuaConverterCandidates::LuaConverterCandidates (Editor *editor)
{
    m_editor = editor;
}

gboolean
LuaConverterCandidates::setLuaPlugin (IBusEnginePlugin * plugin)
{
    m_lua_plugin = plugin;
    return TRUE;
}

gboolean
LuaConverterCandidates::setConverter (const char * lua_function_name)
{
    return ibus_engine_plugin_set_converter (m_lua_plugin, lua_function_name);
}

gboolean
LuaConverterCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    if (!m_lua_plugin)
        return FALSE;

    m_candidates.clear ();

    const char * converter = ibus_engine_plugin_get_converter (m_lua_plugin);

    if (NULL == converter)
        return FALSE;

    for (guint i = 0; i < candidates.size (); i++) {
        EnhancedCandidate & enhanced = candidates[i];

        m_candidates.push_back (enhanced);

        enhanced.m_candidate_type = CANDIDATE_LUA_CONVERTER;
        enhanced.m_candidate_id = i;

        ibus_engine_plugin_call (m_lua_plugin, converter,
                                enhanced.m_display_string.c_str ());
        gchar * string = ibus_engine_plugin_get_first_result (m_lua_plugin);
        enhanced.m_display_string = string;
        g_free (string);
    }

    return TRUE;
}

int
LuaConverterCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    guint id = enhanced.m_candidate_id;
    assert (CANDIDATE_LUA_CONVERTER == enhanced.m_candidate_type);

    const char * converter = ibus_engine_plugin_get_converter (m_lua_plugin);
    assert (NULL != converter);

    if (G_UNLIKELY (id >= m_candidates.size ()))
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    int action = m_editor->selectCandidateInternal (m_candidates[id]);

    if (action & SELECT_CANDIDATE_MODIFY_IN_PLACE) {
        ibus_engine_plugin_call (m_lua_plugin, converter,
                                 enhanced.m_display_string.c_str ());
        gchar * string = ibus_engine_plugin_get_first_result (m_lua_plugin);
        enhanced.m_display_string = string;
        g_free (string);
    }

    return action;
}

gboolean
LuaConverterCandidates::removeCandidate (EnhancedCandidate & enhanced)
{
    guint id = enhanced.m_candidate_id;
    assert (CANDIDATE_LUA_CONVERTER == enhanced.m_candidate_type);

    if (G_UNLIKELY (id >= m_candidates.size ()))
        return FALSE;

    return m_editor->removeCandidateInternal (m_candidates[id]);
}
