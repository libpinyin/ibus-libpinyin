/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
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
#include "PYPinyinProperties.h"
#include <libintl.h>
#include "PYText.h"
#include "PYConfig.h"
#ifdef IBUS_BUILD_LUA_EXTENSION
#include "lua-plugin.h"
#endif

namespace PY {

#define _(text) (dgettext (GETTEXT_PACKAGE, text))
#define N_(text) text

PinyinProperties::PinyinProperties (Config & config)
    : m_config (config),
      m_mode_chinese (m_config.initChinese ()),
      m_mode_full (m_config.initFull ()),
      m_mode_full_punct (m_config.initFullPunct ()),
      m_mode_simp (m_config.initSimpChinese ()),
      m_prop_chinese ("InputMode",
                PROP_TYPE_NORMAL,
                StaticText (m_mode_chinese ?
                            _("Chinese") :
                            _("English")),
                m_mode_chinese ?
                    PKGDATADIR"/icons/chinese.svg" :
                    PKGDATADIR"/icons/english.svg",
                StaticText (m_mode_chinese ?
                            _("Switch to English Mode") :
                            _("Switch to Chinese Mode"))),
      m_prop_full ("mode.full",
                PROP_TYPE_NORMAL,
                StaticText (m_mode_full ?
                            _("Full Width Letter") :
                            _("Half Width Letter")),
                m_mode_full ?
                    PKGDATADIR"/icons/full.svg" :
                    PKGDATADIR"/icons/half.svg",
                StaticText (m_mode_full ?
                            _("Switch to Half Width Letter Mode"):
                            _("Switch to Full Width Letter Mode"))),
      m_prop_full_punct ("mode.full_punct",
                PROP_TYPE_NORMAL,
                StaticText (m_mode_full_punct ?
                            _("Full Width Punct") :
                            _("Half Width Punct")),
                m_mode_full_punct ?
                    PKGDATADIR"/icons/full-punct.svg" :
                    PKGDATADIR"/icons/half-punct.svg",
                StaticText (m_mode_full_punct ?
                            _("Switch to Half Width Punctuation Mode"):
                            _("Switch to Full Width Punctuation Mode"))),
      m_prop_simp ( "mode.simp",
                PROP_TYPE_NORMAL,
                StaticText (m_mode_simp ?
                            _("Simplified Chinese") :
                            _("Traditional Chinese")),
                m_mode_simp ?
                    PKGDATADIR"/icons/simp-chinese.svg" :
                    PKGDATADIR"/icons/trad-chinese.svg",
                StaticText (m_mode_simp ?
                            _("Switch to Traditional Chinese Mode"):
                            _("Switch to Simplfied Chinese Mode"))),
      m_prop_setup ("setup",
                PROP_TYPE_NORMAL,
                StaticText (_("Preferences")),
                "ibus-setup",
                StaticText (_("Preferences")))
{
    if (m_mode_chinese)
        m_prop_chinese.setSymbol(N_("中"));
    else
        m_prop_chinese.setSymbol(N_("英"));

    m_props.append (m_prop_chinese);
    m_props.append (m_prop_full);
    m_props.append (m_prop_full_punct);
    m_props.append (m_prop_simp);
    m_props.append (m_prop_setup);

#ifdef IBUS_BUILD_LUA_EXTENSION
    m_prop_lua_converter = NULL;
#endif
}

PinyinProperties::~PinyinProperties (void)
{
#ifdef IBUS_BUILD_LUA_EXTENSION
    if (m_prop_lua_converter) {
        delete m_prop_lua_converter;
        m_prop_lua_converter = NULL;
    }

    for (auto iter = m_props_lua_converter_vec.begin ();
         iter != m_props_lua_converter_vec.end (); ++iter) {
        delete *iter;
    }

    for (auto iter = m_lua_converter_names.begin ();
         iter != m_lua_converter_names.end (); ++iter) {
        delete *iter;
    }
#endif
}

void
PinyinProperties::toggleModeChinese (void)
{
    m_mode_chinese = ! m_mode_chinese;
    m_prop_chinese.setLabel (m_mode_chinese ?
                             _("Chinese") :
                             _("English"));

    if (m_mode_chinese)
        m_prop_chinese.setSymbol(N_("中"));
    else
        m_prop_chinese.setSymbol(N_("英"));

    m_prop_chinese.setIcon (m_mode_chinese ?
                            PKGDATADIR"/icons/chinese.svg" :
                            PKGDATADIR"/icons/english.svg");
    m_prop_chinese.setTooltip (m_mode_chinese ?
                               _("Switch to English Mode") :
                               _("Switch to Chinese Mode"));
    updateProperty (m_prop_chinese);
    
    m_prop_full_punct.setSensitive (m_mode_chinese);
    updateProperty (m_prop_full_punct);

    m_prop_simp.setSensitive (m_mode_chinese);
    updateProperty (m_prop_simp);
}

void
PinyinProperties::toggleModeFull (void)
{
    m_mode_full = !m_mode_full;
    m_prop_full.setLabel (m_mode_full ?
                          _("Full Width Letter") :
                          _("Half Width Letter"));
    m_prop_full.setIcon (m_mode_full ?
                         PKGDATADIR"/icons/full.svg" :
                         PKGDATADIR"/icons/half.svg");
    m_prop_full.setTooltip (m_mode_full ?
                            _("Switch to Half Width Letter Mode"):
                            _("Switch to Full Width Letter Mode"));
    updateProperty (m_prop_full);
}

void
PinyinProperties::toggleModeFullPunct (void)
{
    m_mode_full_punct = !m_mode_full_punct;
    m_prop_full_punct.setLabel (m_mode_full_punct ?
                                _("Full Width Punct") :
                                _("Half Width Punct"));
    m_prop_full_punct.setIcon (m_mode_full_punct ?
                                PKGDATADIR"/icons/full-punct.svg" :
                                PKGDATADIR"/icons/half-punct.svg");
    m_prop_full_punct.setTooltip(m_mode_full_punct ?
                                 _("Switch to Half Width Punctuation Mode"):
                                 _("Switch to Full Width Punctuation Mode"));
    updateProperty (m_prop_full_punct);
}

void
PinyinProperties::toggleModeSimp (void)
{
    m_mode_simp = ! m_mode_simp;
    m_prop_simp.setLabel (m_mode_simp ?
                          _("Simplified Chinese") :
                          _("Traditional Chinese"));
    m_prop_simp.setIcon (m_mode_simp ?
                            PKGDATADIR"/icons/simp-chinese.svg" :
                            PKGDATADIR"/icons/trad-chinese.svg");
    m_prop_simp.setTooltip(m_mode_simp ?
                           _("Switch to Traditional Chinese Mode"):
                           _("Switch to Simplfied Chinese Mode"));
    updateProperty (m_prop_simp);
}

void
PinyinProperties::reset (void)
{
    if (modeChinese () != m_config.initChinese ()) {
        toggleModeChinese ();
    }
    if (modeFull () != m_config.initFull ()) {
        toggleModeFull ();
    }
    if (modeFullPunct () != m_config.initFullPunct ()) {
        toggleModeFullPunct ();
    }
    if (modeSimp () != m_config.initSimpChinese ()) {
        toggleModeSimp ();
    }
}

gboolean
PinyinProperties::propertyActivate (const gchar *prop_name, guint prop_state) {
    const static std::string mode_chinese ("InputMode");
    const static std::string mode_full ("mode.full");
    const static std::string mode_full_punct ("mode.full_punct");
    const static std::string mode_simp ("mode.simp");

    if (mode_chinese == prop_name) {
        toggleModeChinese ();
        return TRUE;
    }
    else if (mode_full == prop_name) {
        toggleModeFull ();
        return TRUE;
    }
    else if (mode_full_punct == prop_name) {
        toggleModeFullPunct ();
        return TRUE;
    }
    else if (mode_simp == prop_name) {
        toggleModeSimp ();
        return TRUE;
    }

#ifdef IBUS_BUILD_LUA_EXTENSION
    const int len = strlen("LuaConverter.");
    if (0 == strncmp (prop_name, "LuaConverter.", len)) {
        toggleLuaConverter (len, prop_name, prop_state);
        return TRUE;
    }
#endif

    return FALSE;
}

#ifdef IBUS_BUILD_LUA_EXTENSION
gboolean
PinyinProperties::setLuaPlugin (IBusEnginePlugin *plugin) {
    m_lua_plugin = plugin;
    return TRUE;
}

void
PinyinProperties::toggleLuaConverter (const int prefix_len,
                                      const gchar *prop_name,
                                      guint prop_state) {
    if (prop_state == PROP_STATE_CHECKED) {
        std::string name = prop_name + prefix_len;
        if (name == "None")
            m_config.luaConverter ("");
        else
            m_config.luaConverter (name);
    }

    for (auto iter = m_props_lua_converter_vec.begin ();
         iter != m_props_lua_converter_vec.end (); ++iter) {
        Property *prop = *iter;
        if (0 == g_strcmp0 (prop->getKey (), prop_name)) {
            prop->setState ((IBusPropState) prop_state);
            updateProperty (*prop);
        }
    }
}

gboolean
PinyinProperties::appendLuaConverter (void) {
    if (!m_lua_plugin)
        return FALSE;

    const GArray * converters =
        ibus_engine_plugin_get_available_converters (m_lua_plugin);
    if (converters->len == 0)
        return FALSE;

    m_prop_lua_converter = new Property ("LuaConverter",
                                         PROP_TYPE_MENU,
                                         StaticText (_("Lua Converter")),
                                         PKGDATADIR"/icons/lua-converter.svg",
                                         StaticText (_("Use the Lua Convertor")));

    Property * prop = NULL;

    /* Add the None converter. */
    prop = new Property ("LuaConverter.None",
                         PROP_TYPE_RADIO,
                         StaticText (_("None")));

    if (m_config.luaConverter ().empty ())
        prop->setState (PROP_STATE_CHECKED);

    m_props_lua_converter_vec.push_back (prop);
    m_props_lua_converter_list.append (*prop);

    /* Add the User Lua Converters. */
    for (int i = 0; i < converters->len; i++) {
        lua_converter_t *converter = &g_array_index
            (converters, lua_converter_t, i);

        const gchar *name = converter->lua_function_name;
        gchar *key = g_strdup_printf ("LuaConverter.%s", name);
        m_lua_converter_names.push_back (key);

        prop = new Property (key,
                             PROP_TYPE_RADIO,
                             Text (converter->description));

        if (!m_config.luaConverter ().empty () &&
            m_config.luaConverter () == name)
            prop->setState (PROP_STATE_CHECKED);

        m_props_lua_converter_vec.push_back (prop);
        m_props_lua_converter_list.append (*prop);
    }

    m_prop_lua_converter->setSubProps (m_props_lua_converter_list);
    m_props.append (*m_prop_lua_converter);

    return TRUE;
}

#endif

};
