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

#ifndef __PY_LIB_PINYIN_LUA_CONVERTER_CANDIDATES_H_
#define __PY_LIB_PINYIN_LUA_CONVERTER_CANDIDATES_H_

#include "lua-plugin.h"

#include <vector>
#include "PYPointer.h"
#include "PYPEnhancedCandidates.h"

namespace PY {

class Editor;

class LuaConverterCandidates : public EnhancedCandidates<Editor> {
public:
    LuaConverterCandidates (Editor *editor);

public:
    gboolean setLuaPlugin (IBusEnginePlugin * plugin);

    gboolean setConverter (const char * lua_function_name);

    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);

    int selectCandidate (EnhancedCandidate & enhanced);
    gboolean removeCandidate (EnhancedCandidate & enhanced);

protected:
    std::vector<EnhancedCandidate> m_candidates;

    Pointer<IBusEnginePlugin> m_lua_plugin;
};

};

#endif
