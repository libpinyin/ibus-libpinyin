/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
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
#ifndef __PY_CONFIG_H_
#define __PY_CONFIG_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string>
#include <gio/gio.h>
#include <ibus.h>
#include <pinyin.h>
#include "PYUtil.h"
#include "PYObject.h"

namespace PY {

enum CloudInputSource{
    BAIDU = 0,
    GOOGLE
};

class Config {
protected:
    Config (const std::string & name);
    virtual ~Config (void);

public:
    std::string dictionaries (void) const       { return m_dictionaries; }
    std::string luaConverter (void) const       { return m_lua_converter; }
    pinyin_option_t option (void) const         { return m_option & m_option_mask; }
    guint orientation (void) const              { return m_orientation; }
    guint pageSize (void) const                 { return m_page_size; }
    gboolean rememberEveryInput (void) const    { return m_remember_every_input; }
    sort_option_t sortOption (void) const       { return m_sort_option; }
    gboolean showSuggestion (void) const        { return m_show_suggestion; }
    gboolean shiftSelectCandidate (void) const  { return m_shift_select_candidate; }
    gboolean minusEqualPage (void) const        { return m_minus_equal_page; }
    gboolean commaPeriodPage (void) const       { return m_comma_period_page; }
    gboolean autoCommit (void) const            { return m_auto_commit; }
    gboolean doublePinyin (void) const          { return m_double_pinyin; }
    DoublePinyinScheme doublePinyinSchema (void) const { return m_double_pinyin_schema; }
    gboolean initChinese (void) const           { return m_init_chinese; }
    gboolean initFull (void) const              { return m_init_full; }
    gboolean initFullPunct (void) const         { return m_init_full_punct; }
    gboolean initSimpChinese (void) const       { return m_init_simp_chinese; }
    ZhuyinScheme bopomofoKeyboardMapping (void) const   { return m_bopomofo_keyboard_mapping; }
    gint selectKeys (void) const                { return m_select_keys; }
    gboolean guideKey (void) const              { return m_guide_key; }
    gboolean auxiliarySelectKeyF (void) const   { return m_auxiliary_select_key_f; }
    gboolean auxiliarySelectKeyKP (void) const  { return m_auxiliary_select_key_kp; }
    gboolean enterKey (void) const  { return m_enter_key; }

    std::string mainSwitch (void) const         { return m_main_switch; }
    std::string letterSwitch (void) const       { return m_letter_switch; }
    std::string punctSwitch (void) const        { return m_punct_switch; }
    std::string tradSwitch (void) const         { return m_trad_switch; }

    gboolean enableCloudInput (void) const      { return m_enable_cloud_input; }
    guint cloudInputSource (void) const         { return m_cloud_input_source; }
    guint minCloudInputTriggerLen (void) const  { return m_min_cloud_input_trigger_len; }
    guint cloudCandidatesNumber (void) const    { return m_cloud_candidates_number; }
    guint firstCloudCandidatePos (void) const   { return m_first_cloud_candidate_pos; }
    
    void disableCloudInput(void);

protected:
    bool read (const gchar * name, bool defval);
    gint read (const gchar * name, gint defval);
    std::string read (const gchar * name, const gchar * defval);
    void initDefaultValues (void);

    virtual void readDefaultValues (void);
    virtual gboolean valueChanged (const std::string  &schema_id,
                                   const std::string  &name,
                                   GVariant           *value);
private:
    static void valueChangedCallback (GSettings      *settings,
                                      const gchar    *name,
                                      Config         *self);

protected:
    GSettings *m_settings;
    std::string m_schema_id;
    std::string m_dictionaries;
    std::string m_lua_converter;
    pinyin_option_t m_option;
    pinyin_option_t m_option_mask;

    gint m_orientation;
    guint m_page_size;
    gboolean m_remember_every_input;
    sort_option_t m_sort_option;
    gboolean m_show_suggestion;

    gboolean m_shift_select_candidate;
    gboolean m_minus_equal_page;
    gboolean m_comma_period_page;
    gboolean m_auto_commit;

    gboolean m_double_pinyin;
    DoublePinyinScheme m_double_pinyin_schema;

    gboolean m_init_chinese;
    gboolean m_init_full;
    gboolean m_init_full_punct;
    gboolean m_init_simp_chinese;

    ZhuyinScheme m_bopomofo_keyboard_mapping;
    gint m_select_keys;
    gboolean m_guide_key;
    gboolean m_auxiliary_select_key_f;
    gboolean m_auxiliary_select_key_kp;

    gboolean m_enter_key;

    std::string m_main_switch;
    std::string m_letter_switch;
    std::string m_punct_switch;
    std::string m_trad_switch;

    gboolean m_enable_cloud_input;
    guint m_cloud_input_source;
    guint m_min_cloud_input_trigger_len;
    guint m_cloud_candidates_number;
    guint m_first_cloud_candidate_pos;
};


static inline bool
normalizeGVariant (GVariant *value, bool defval)
{
    if (value == NULL ||
        g_variant_classify (value) != G_VARIANT_CLASS_BOOLEAN) {
        g_warn_if_reached ();
        return defval;
    }
    return g_variant_get_boolean (value);
}

static inline gint
normalizeGVariant (GVariant *value, gint defval)
{
    if (value == NULL ||
        g_variant_classify (value) != G_VARIANT_CLASS_INT32) {
        g_warn_if_reached ();
        return defval;
    }
    return g_variant_get_int32 (value);
}

static inline std::string
normalizeGVariant (GVariant *value, const std::string &defval)
{
    if (value == NULL ||
        g_variant_classify (value) != G_VARIANT_CLASS_STRING) {
        g_warn_if_reached ();
        return defval;
    }
    return g_variant_get_string (value, NULL);
}

};
#endif
