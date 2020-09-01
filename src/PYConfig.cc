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
#include "PYConfig.h"

#include "PYTypes.h"
#include "PYBus.h"

namespace PY {


Config::Config (const std::string & name)
    : m_schema_id (name)
{
    m_settings = NULL;
    initDefaultValues ();
}

Config::~Config (void)
{
}

void
Config::initDefaultValues (void)
{
    m_orientation = IBUS_ORIENTATION_HORIZONTAL;
    m_page_size = 5;
    m_display_style = DISPLAY_STYLE_TRADITIONAL;
    m_remember_every_input = FALSE;
    m_sort_option = SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY;
    m_show_suggestion = FALSE;
    m_emoji_candidate = TRUE;

    m_shift_select_candidate = FALSE;
    m_minus_equal_page = TRUE;
    m_comma_period_page = TRUE;
    m_auto_commit = FALSE;

    m_double_pinyin = FALSE;
    m_double_pinyin_schema = DOUBLE_PINYIN_DEFAULT;

    m_init_chinese = TRUE;
    m_init_full = FALSE;
    m_init_full_punct = TRUE;
    m_init_simp_chinese = TRUE;

    m_dictionaries = "";
    m_lua_converter = "";
    m_opencc_config = "s2t.json";

    m_main_switch = "<Shift>";
    m_letter_switch = "";
    m_punct_switch = "<Control>period";
    m_both_switch = "";
    m_trad_switch = "<Control><Shift>f";

    m_network_dictionary_start_timestamp = 0;
    m_network_dictionary_end_timestamp = 0;

    m_enable_cloud_input = FALSE;
    m_cloud_candidates_number = 1;
    m_cloud_input_source = BAIDU;
    m_cloud_request_delay_time = 600;
}


void
Config::readDefaultValues (void)
{
}

bool
Config::read (const gchar * name,
              bool          defval)
{
    GVariant *value = NULL;
    if ((value = g_settings_get_value (m_settings, name)) != NULL) {
        if (g_variant_classify (value) == G_VARIANT_CLASS_BOOLEAN)
            return g_variant_get_boolean (value);
    }

    g_warn_if_reached ();
    return defval;
}

gint
Config::read (const gchar * name,
              gint          defval)
{
    GVariant *value = NULL;
    if ((value = g_settings_get_value (m_settings, name)) != NULL) {
        if (g_variant_classify (value) == G_VARIANT_CLASS_INT32)
            return g_variant_get_int32 (value);
    }

    g_warn_if_reached ();
    return defval;
}

std::string
Config::read (const gchar * name,
              const gchar * defval)
{
    GVariant *value = NULL;
    if ((value = g_settings_get_value (m_settings, name)) != NULL) {
        if (g_variant_classify (value) == G_VARIANT_CLASS_STRING)
            return g_variant_get_string (value, NULL);
    }

    g_warn_if_reached ();
    return defval;
}

gint64
Config::read (const gchar * name,
              gint64        defval)
{
    GVariant *value = NULL;
    if ((value = g_settings_get_value (m_settings, name)) != NULL) {
        if (g_variant_classify (value) == G_VARIANT_CLASS_INT64)
            return g_variant_get_int64 (value);
    }

    g_warn_if_reached ();
    return defval;
}

gboolean
Config::write (const gchar * name, bool val)
{
    return g_settings_set_boolean (m_settings, name, val);
}

gboolean
Config::write (const gchar * name, gint val)
{
    return g_settings_set_int (m_settings, name, val);
}

gboolean
Config::write (const gchar * name, const gchar * val)
{
    return g_settings_set_string (m_settings, name, val);
}

gboolean
Config::write (const gchar * name, gint64 val)
{
    return g_settings_set_int64 (m_settings, name, val);
}

gboolean
Config::valueChanged (const std::string &schema_id,
                      const std::string &name,
                      GVariant          *value)
{
    return FALSE;
}

void
Config::valueChangedCallback (GSettings   *settings,
                              const gchar *name,
                              Config      *self)
{
    gchar * property = NULL;
    g_object_get (settings, "schema-id", &property, NULL);
    std::string schema_id (property);
    g_free (property);

    GVariant * value = g_settings_get_value (settings, name);
    self->valueChanged (schema_id, name, value);
    g_variant_unref (value);
}

};
