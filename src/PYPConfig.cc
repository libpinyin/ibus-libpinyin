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
#include "PYPConfig.h"

#include <string.h>
#include <pinyin.h>
#include "PYBus.h"
#include "PYLibPinyin.h"

#define USE_G_SETTINGS_LIST_KEYS 0

namespace PY {

const gchar * const CONFIG_CORRECT_PINYIN            = "correct-pinyin";
const gchar * const CONFIG_FUZZY_PINYIN              = "fuzzy-pinyin";
const gchar * const CONFIG_ORIENTATION               = "lookup-table-orientation";
const gchar * const CONFIG_PAGE_SIZE                 = "lookup-table-page-size";
const gchar * const CONFIG_REMEMBER_EVERY_INPUT      = "remember-every-input";
const gchar * const CONFIG_SORT_OPTION               = "sort-candidate-option";
const gchar * const CONFIG_SHOW_SUGGESTION           = "show-suggestion";
const gchar * const CONFIG_SHIFT_SELECT_CANDIDATE    = "shift-select-candidate";
const gchar * const CONFIG_MINUS_EQUAL_PAGE          = "minus-equal-page";
const gchar * const CONFIG_COMMA_PERIOD_PAGE         = "comma-period-page";
const gchar * const CONFIG_AUTO_COMMIT               = "auto-commit";
const gchar * const CONFIG_DOUBLE_PINYIN             = "double-pinyin";
const gchar * const CONFIG_DOUBLE_PINYIN_SCHEMA      = "double-pinyin-schema";
const gchar * const CONFIG_INIT_CHINESE              = "init-chinese";
const gchar * const CONFIG_INIT_FULL                 = "init-full";
const gchar * const CONFIG_INIT_FULL_PUNCT           = "init-full-punct";
const gchar * const CONFIG_INIT_SIMP_CHINESE         = "init-simplified-chinese";
const gchar * const CONFIG_DICTIONARIES              = "dictionaries";
const gchar * const CONFIG_LUA_CONVERTER             = "lua-converter";
const gchar * const CONFIG_BOPOMOFO_KEYBOARD_MAPPING = "bopomofo-keyboard-mapping";
const gchar * const CONFIG_SELECT_KEYS               = "select-keys";
const gchar * const CONFIG_GUIDE_KEY                 = "guide-key";
const gchar * const CONFIG_AUXILIARY_SELECT_KEY_F    = "auxiliary-select-key-f";
const gchar * const CONFIG_AUXILIARY_SELECT_KEY_KP   = "auxiliary-select-key-kp";
const gchar * const CONFIG_ENTER_KEY                 = "enter-key";
const gchar * const CONFIG_IMPORT_DICTIONARY         = "import-dictionary";
const gchar * const CONFIG_EXPORT_DICTIONARY         = "export-dictionary";
const gchar * const CONFIG_CLEAR_USER_DATA           = "clear-user-data";
/* const gchar * const CONFIG_CTRL_SWITCH               = "ctrl-switch"; */
const gchar * const CONFIG_MAIN_SWITCH               = "main-switch";
const gchar * const CONFIG_LETTER_SWITCH             = "letter-switch";
const gchar * const CONFIG_PUNCT_SWITCH              = "punct-switch";
const gchar * const CONFIG_TRAD_SWITCH               = "trad-switch";
const gchar * const CONFIG_INIT_ENABLE_CLOUD_INPUT   = "enable-cloud-input";
const gchar * const CONFIG_CLOUD_INPUT_SOURCE        = "cloud-input-source";   
const gchar * const CONFIG_CLOUD_CANDIDATES_NUMBER   = "cloud-candidates-number";
const gchar * const CONFIG_MINIMUM_TRIGGER_LENGTH    = "minimum-cloud-input-trigger-length";
const gchar * const CONFIG_FIRST_CLOUD_CANDIDATE_POS = "first-cloud-candidate-position";

const pinyin_option_t PINYIN_DEFAULT_OPTION =
        PINYIN_INCOMPLETE |
        ZHUYIN_INCOMPLETE|
        PINYIN_CORRECT_ALL|
        0;

std::unique_ptr<PinyinConfig> PinyinConfig::m_instance;
std::unique_ptr<BopomofoConfig> BopomofoConfig::m_instance;

LibPinyinConfig::LibPinyinConfig (const std::string & name)
    : Config (name)
{
    m_settings = g_settings_new (m_schema_id.c_str ());
    initDefaultValues ();
    g_signal_connect (m_settings,
                      "changed",
                      G_CALLBACK (valueChangedCallback),
                      this);
}

LibPinyinConfig::~LibPinyinConfig (void)
{
    g_object_unref (m_settings);
    m_settings = NULL;
}

void
LibPinyinConfig::initDefaultValues (void)
{
    m_option = PINYIN_DEFAULT_OPTION;
    m_option_mask = PINYIN_INCOMPLETE | ZHUYIN_INCOMPLETE | PINYIN_CORRECT_ALL;

    m_orientation = IBUS_ORIENTATION_HORIZONTAL;
    m_page_size = 5;
    m_remember_every_input = FALSE;
    m_sort_option = SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY;
    m_show_suggestion = FALSE;

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

    m_bopomofo_keyboard_mapping = ZHUYIN_DEFAULT;

    m_dictionaries = "";
    m_lua_converter = "";

    m_main_switch = "<Shift>";
    m_letter_switch = "";
    m_punct_switch = "<Control>period";
    m_trad_switch = "<Control><Shift>f";

    m_enable_cloud_input = FALSE;
    m_min_cloud_input_trigger_len = 2;
    m_cloud_candidates_number = 1;
    m_first_cloud_candidate_pos = 3;
    m_cloud_input_source = BAIDU;
}

static const struct {
    const gchar * const name;
    guint option;
} options [] = {
    { "incomplete-pinyin",       PINYIN_INCOMPLETE|ZHUYIN_INCOMPLETE},
    /* fuzzy pinyin */
    { "fuzzy-pinyin-c-ch",       PINYIN_AMB_C_CH      },
    { "fuzzy-pinyin-z-zh",       PINYIN_AMB_Z_ZH      },
    { "fuzzy-pinyin-s-sh",       PINYIN_AMB_S_SH      },
    { "fuzzy-pinyin-l-n",        PINYIN_AMB_L_N       },
    { "fuzzy-pinyin-f-h",        PINYIN_AMB_F_H       },
    { "fuzzy-pinyin-l-r",        PINYIN_AMB_L_R       },
    { "fuzzy-pinyin-g-k",        PINYIN_AMB_G_K       },
    { "fuzzy-pinyin-an-ang",     PINYIN_AMB_AN_ANG    },
    { "fuzzy-pinyin-en-eng",     PINYIN_AMB_EN_ENG    },
    { "fuzzy-pinyin-in-ing",     PINYIN_AMB_IN_ING    },
    /* dynamic adjust */
    { "dynamic-adjust",          DYNAMIC_ADJUST       },
};

static const struct{
    gint sort_option_index;
    sort_option_t sort_option;
} sort_options [] = {
    {0, SORT_BY_PHRASE_LENGTH_AND_FREQUENCY},
    {1, SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY}
};

void
LibPinyinConfig::readDefaultValues (void)
{
#if USE_G_SETTINGS_LIST_KEYS
    /* read all values together */
    initDefaultValues ();
    gchar **keys = g_settings_list_keys (m_settings);
    g_return_if_fail (keys != NULL);

    for (gchar **iter = keys; *iter != NULL; ++iter) {
        gchar *name = *iter;

        /* skip signals here. */
        if (0 == strcmp(CONFIG_IMPORT_DICTIONARY, name))
            continue;

        if (0 == strcmp(CONFIG_EXPORT_DICTIONARY, name))
            continue;

        if (0 == strcmp(CONFIG_CLEAR_USER_DATA, name))
            continue;

        GVariant *value = g_settings_get_value (m_settings, name);
        valueChanged (m_schema_id, name, value);
        g_variant_unref (value);
    }

    g_strfreev (keys);
#else
    /* others */
    m_orientation = read (CONFIG_ORIENTATION, 0);
    if (m_orientation != IBUS_ORIENTATION_VERTICAL &&
        m_orientation != IBUS_ORIENTATION_HORIZONTAL) {
        m_orientation = IBUS_ORIENTATION_HORIZONTAL;
        g_warn_if_reached ();
    }
    m_page_size = read (CONFIG_PAGE_SIZE, 5);
    if (m_page_size > 10) {
        m_page_size = 5;
        g_warn_if_reached ();
    }
    m_remember_every_input = read (CONFIG_REMEMBER_EVERY_INPUT, false);

    const gint index = read (CONFIG_SORT_OPTION, 0);
    m_sort_option = SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY;

    for (guint i = 0; i < G_N_ELEMENTS (sort_options); i++) {
        if (index == sort_options[i].sort_option_index) {
            /* set sort option. */
            m_sort_option = sort_options[i].sort_option;
        }
    }

    m_show_suggestion = read (CONFIG_SHOW_SUGGESTION, false);

    m_dictionaries = read (CONFIG_DICTIONARIES, "");

    m_main_switch = read (CONFIG_MAIN_SWITCH, "<Shift>");
    m_letter_switch = read (CONFIG_LETTER_SWITCH, "");
    m_punct_switch = read (CONFIG_PUNCT_SWITCH, "<Control>period");
    m_trad_switch = read (CONFIG_TRAD_SWITCH, "<Control><Shift>f");

    /* fuzzy pinyin */
    if (read (CONFIG_FUZZY_PINYIN, false))
        m_option_mask |= PINYIN_AMB_ALL;
    else
        m_option_mask &= ~PINYIN_AMB_ALL;

    /* read values */
    for (guint i = 0; i < G_N_ELEMENTS (options); i++) {
        if (read (options[i].name,
                  (options[i].option & PINYIN_DEFAULT_OPTION) != 0)) {
            m_option |= options[i].option;
        }
        else {
            m_option &= ~options[i].option;
        }
    }
    
    m_enable_cloud_input = read (CONFIG_INIT_ENABLE_CLOUD_INPUT, false);
#endif
}

gboolean
LibPinyinConfig::valueChanged (const std::string &schema_id,
                               const std::string &name,
                               GVariant          *value)
{
    if (m_schema_id != schema_id)
        return FALSE;

    /* lookup table page size */
    if (CONFIG_ORIENTATION == name) {
        m_orientation = normalizeGVariant (value, IBUS_ORIENTATION_HORIZONTAL);
        if (m_orientation != IBUS_ORIENTATION_VERTICAL &&
            m_orientation != IBUS_ORIENTATION_HORIZONTAL) {
            m_orientation = IBUS_ORIENTATION_HORIZONTAL;
            g_warn_if_reached ();
        }
    }
    else if (CONFIG_PAGE_SIZE == name) {
        m_page_size = normalizeGVariant (value, 5);
        if (m_page_size > 10) {
            m_page_size = 5;
            g_warn_if_reached ();
        }
    } else if (CONFIG_REMEMBER_EVERY_INPUT == name) {
        m_remember_every_input = normalizeGVariant (value, false);
    } else if (CONFIG_SORT_OPTION == name) {
        const gint index = normalizeGVariant (value, 0);
        m_sort_option = SORT_BY_PHRASE_LENGTH_AND_PINYIN_LENGTH_AND_FREQUENCY;

        for (guint i = 0; i < G_N_ELEMENTS (sort_options); i++) {
            if (index == sort_options[i].sort_option_index) {
                /* set sort option. */
                m_sort_option = sort_options[i].sort_option;
            }
        }
    } else if (CONFIG_SHOW_SUGGESTION == name) {
        m_show_suggestion = normalizeGVariant (value, false);
    } else if (CONFIG_DICTIONARIES == name) {
        m_dictionaries = normalizeGVariant (value, std::string (""));
    } else if (CONFIG_MAIN_SWITCH == name) {
        m_main_switch = normalizeGVariant (value, std::string ("<Shift>"));
    } else if (CONFIG_LETTER_SWITCH == name) {
        m_letter_switch = normalizeGVariant (value, std::string (""));
    } else if (CONFIG_PUNCT_SWITCH == name) {
        m_punct_switch = normalizeGVariant (value, std::string ("<Control>period"));
    } else if (CONFIG_TRAD_SWITCH == name) {
        m_trad_switch = normalizeGVariant (value, std::string ("<Control><Shift>f"));
    }
    /*cloud input*/
    else if (CONFIG_INIT_ENABLE_CLOUD_INPUT == name) {
        m_enable_cloud_input = normalizeGVariant (value, false);
    }
    /* fuzzy pinyin */
    else if (CONFIG_FUZZY_PINYIN == name) {
        if (normalizeGVariant (value, false))
            m_option_mask |= PINYIN_AMB_ALL;
        else
            m_option_mask &= ~PINYIN_AMB_ALL;
    }
    else {
        for (guint i = 0; i < G_N_ELEMENTS (options); i++) {
            if (G_LIKELY (options[i].name != name))
                continue;
            if (normalizeGVariant (value,
                    (options[i].option & PINYIN_DEFAULT_OPTION) != 0))
                m_option |= options[i].option;
            else
                m_option &= ~options[i].option;
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

void
LibPinyinConfig::valueChangedCallback (GSettings   *settings,
                                       const gchar *name,
                                       LibPinyinConfig *self)
{

    gchar * property = NULL;
    g_object_get (settings, "schema-id", &property, NULL);
    std::string schema_id (property);
    g_free (property);

    if (self->m_schema_id != schema_id)
        return;

    GVariant * value = g_settings_get_value (settings, name);
    self->valueChanged (self->m_schema_id, name, value);
    g_variant_unref (value);

    if (self->m_schema_id == "com.github.libpinyin.ibus-libpinyin.libpinyin")
        LibPinyinBackEnd::instance ().setPinyinOptions (self);
    if (self->m_schema_id == "com.github.libpinyin.ibus-libpinyin.libbopomofo")
        LibPinyinBackEnd::instance ().setChewingOptions (self);
}

static const struct {
    const gchar * const name;
    guint option;
} pinyin_options [] = {
    /* correct */
    { "correct-pinyin-gn-ng",    PINYIN_CORRECT_GN_NG    },
    { "correct-pinyin-mg-ng",    PINYIN_CORRECT_MG_NG    },
    { "correct-pinyin-iou-iu",   PINYIN_CORRECT_IOU_IU   },
    { "correct-pinyin-uei-ui",   PINYIN_CORRECT_UEI_UI   },
    { "correct-pinyin-uen-un",   PINYIN_CORRECT_UEN_UN   },
    { "correct-pinyin-ue-ve",    PINYIN_CORRECT_UE_VE    },
    { "correct-pinyin-v-u",      PINYIN_CORRECT_V_U      },
    { "correct-pinyin-on-ong",   PINYIN_CORRECT_ON_ONG   },
};

/* Here are the double pinyin keyboard scheme mapping table. */
static const struct{
    gint double_pinyin_keyboard;
    DoublePinyinScheme scheme;
} double_pinyin_schemes [] = {
    {0, DOUBLE_PINYIN_MS},
    {1, DOUBLE_PINYIN_ZRM},
    {2, DOUBLE_PINYIN_ABC},
    {3, DOUBLE_PINYIN_ZIGUANG},
    {4, DOUBLE_PINYIN_PYJJ},
    {5, DOUBLE_PINYIN_XHE}
};

PinyinConfig::PinyinConfig ()
    : LibPinyinConfig ("com.github.libpinyin.ibus-libpinyin.libpinyin")
{
}

void
PinyinConfig::init ()
{
    if (m_instance.get () == NULL) {
        m_instance.reset (new PinyinConfig ());
        m_instance->readDefaultValues ();
    }
}

void
PinyinConfig::readDefaultValues (void)
{
    LibPinyinConfig::readDefaultValues ();
#if !USE_G_SETTINGS_LIST_KEYS
    /* double pinyin */
    m_double_pinyin = read (CONFIG_DOUBLE_PINYIN, false);

    const gint map = read (CONFIG_DOUBLE_PINYIN_SCHEMA, 0);
    m_double_pinyin_schema = DOUBLE_PINYIN_DEFAULT;

    for (guint i = 0; i < G_N_ELEMENTS (double_pinyin_schemes); i++) {
        if (map == double_pinyin_schemes[i].double_pinyin_keyboard) {
            /* set double pinyin scheme. */
            m_double_pinyin_schema = double_pinyin_schemes[i].scheme;
        }
    }

    /* init states */
    m_init_chinese = read (CONFIG_INIT_CHINESE, true);
    m_init_full = read (CONFIG_INIT_FULL, false);
    m_init_full_punct = read (CONFIG_INIT_FULL_PUNCT, true);
    m_init_simp_chinese = read (CONFIG_INIT_SIMP_CHINESE, true);

    /* other */
    m_shift_select_candidate = read (CONFIG_SHIFT_SELECT_CANDIDATE, false);
    m_minus_equal_page = read (CONFIG_MINUS_EQUAL_PAGE, true);
    m_comma_period_page = read (CONFIG_COMMA_PERIOD_PAGE, true);
    m_auto_commit = read (CONFIG_AUTO_COMMIT, false);

    /* lua */
    m_lua_converter = read (CONFIG_LUA_CONVERTER, "");

    /* correct pinyin */
    if (read (CONFIG_CORRECT_PINYIN, true))
        m_option_mask |= PINYIN_CORRECT_ALL;
    else
        m_option_mask &= ~PINYIN_CORRECT_ALL;

    /* read values */
    for (guint i = 0; i < G_N_ELEMENTS (pinyin_options); i++) {
        if (read (pinyin_options[i].name,
                (pinyin_options[i].option & PINYIN_DEFAULT_OPTION) != 0))
            m_option |= pinyin_options[i].option;
        else
            m_option &= ~pinyin_options[i].option;
    }
    m_min_cloud_input_trigger_len = read (CONFIG_MINIMUM_TRIGGER_LENGTH, 2);
    if (m_min_cloud_input_trigger_len > 10 || m_min_cloud_input_trigger_len < 1) {
        m_min_cloud_input_trigger_len = 2;
        g_warn_if_reached ();
    }
    m_cloud_candidates_number = read (CONFIG_CLOUD_CANDIDATES_NUMBER, 1);
    if (m_cloud_candidates_number > 10 || m_cloud_candidates_number < 1) {
        m_cloud_candidates_number = 1;
        g_warn_if_reached ();
    }
    m_first_cloud_candidate_pos = read (CONFIG_FIRST_CLOUD_CANDIDATE_POS, 2);
    if (m_first_cloud_candidate_pos > 10 || m_first_cloud_candidate_pos < 1) {
        m_first_cloud_candidate_pos = 3;
        g_warn_if_reached ();
    }
    m_cloud_input_source = read (CONFIG_CLOUD_INPUT_SOURCE, 0);
    if (m_cloud_input_source != BAIDU &&
        m_cloud_input_source != GOOGLE) {
        m_cloud_input_source = BAIDU;
        g_warn_if_reached ();
    }
#endif
}

gboolean
PinyinConfig::valueChanged (const std::string &schema_id,
                            const std::string &name,
                            GVariant          *value)
{
    if (m_schema_id != schema_id)
        return FALSE;

    if (LibPinyinConfig::valueChanged (schema_id, name, value))
        return TRUE;

    /* double pinyin */
    if (CONFIG_DOUBLE_PINYIN == name)
        m_double_pinyin = normalizeGVariant (value, false);
    else if (CONFIG_DOUBLE_PINYIN_SCHEMA == name) {
        const gint map = normalizeGVariant (value, 0);
        m_double_pinyin_schema = DOUBLE_PINYIN_DEFAULT;

        for (guint i = 0; i < G_N_ELEMENTS (double_pinyin_schemes); i++) {
            if (map == double_pinyin_schemes[i].double_pinyin_keyboard) {
                /* set double pinyin scheme. */
                m_double_pinyin_schema = double_pinyin_schemes[i].scheme;
            }
        }
    }
    /* init states */
    else if (CONFIG_INIT_CHINESE == name)
        m_init_chinese = normalizeGVariant (value, true);
    else if (CONFIG_INIT_FULL == name)
        m_init_full = normalizeGVariant (value, true);
    else if (CONFIG_INIT_FULL_PUNCT == name)
        m_init_full_punct = normalizeGVariant (value, true);
    else if (CONFIG_INIT_SIMP_CHINESE == name)
        m_init_simp_chinese = normalizeGVariant (value, true);
    /* others */
    else if (CONFIG_SHIFT_SELECT_CANDIDATE == name)
        m_shift_select_candidate = normalizeGVariant (value, false);
    else if (CONFIG_MINUS_EQUAL_PAGE == name)
        m_minus_equal_page = normalizeGVariant (value, true);
    else if (CONFIG_COMMA_PERIOD_PAGE == name)
        m_comma_period_page = normalizeGVariant (value, true);
    else if (CONFIG_LUA_CONVERTER == name)
        m_lua_converter = normalizeGVariant (value, std::string (""));
    else if (CONFIG_AUTO_COMMIT == name)
        m_auto_commit = normalizeGVariant (value, false);
    else if (CONFIG_IMPORT_DICTIONARY == name) {
        std::string filename = normalizeGVariant (value, std::string(""));
        LibPinyinBackEnd::instance ().importPinyinDictionary (filename.c_str ());
    }
    else if (CONFIG_EXPORT_DICTIONARY == name) {
        std::string filename = normalizeGVariant (value, std::string(""));
        LibPinyinBackEnd::instance ().exportPinyinDictionary (filename.c_str ());
    }
    else if (CONFIG_CLEAR_USER_DATA == name) {
        std::string target = normalizeGVariant (value, std::string(""));
        LibPinyinBackEnd::instance ().clearPinyinUserData(target.c_str ());
    } /* correct pinyin */
    else if (CONFIG_CORRECT_PINYIN == name) {
        if (normalizeGVariant (value, true))
            m_option_mask |= PINYIN_CORRECT_ALL;
        else
            m_option_mask &= ~PINYIN_CORRECT_ALL;
    }
    else if (CONFIG_CLOUD_CANDIDATES_NUMBER == name) {
        m_cloud_candidates_number = normalizeGVariant (value, 1);
        if (m_cloud_candidates_number > 10 || m_cloud_candidates_number < 1) {
            m_cloud_candidates_number = 1;
            g_warn_if_reached ();
        }
    }
    else if (CONFIG_MINIMUM_TRIGGER_LENGTH == name) {
        m_min_cloud_input_trigger_len = normalizeGVariant (value, 2);
        if (m_min_cloud_input_trigger_len > 10 || m_min_cloud_input_trigger_len < 1) {
            m_min_cloud_input_trigger_len = 2;
            g_warn_if_reached ();
        }
    } 
    else if (CONFIG_FIRST_CLOUD_CANDIDATE_POS == name) {
        m_first_cloud_candidate_pos = normalizeGVariant (value, 2);
        if (m_first_cloud_candidate_pos > 10 || m_first_cloud_candidate_pos < 1) {
            m_first_cloud_candidate_pos = 3;
            g_warn_if_reached ();
        }
    } 
    else if (CONFIG_CLOUD_INPUT_SOURCE== name) {
        m_cloud_input_source = normalizeGVariant (value, BAIDU);
        if (m_cloud_input_source != BAIDU &&
            m_cloud_input_source != GOOGLE) {
            m_cloud_input_source = BAIDU;
            g_warn_if_reached ();
        }
    }
    else {
        for (guint i = 0; i < G_N_ELEMENTS (pinyin_options); i++) {
            if (G_LIKELY (pinyin_options[i].name != name))
                continue;
            if (normalizeGVariant (value,
                    (pinyin_options[i].option & PINYIN_DEFAULT_OPTION) != 0))
                m_option |= pinyin_options[i].option;
            else
                m_option &= ~pinyin_options[i].option;
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

/* Here are the chewing keyboard scheme mapping table. */
static const struct {
    gint bopomofo_keyboard;
    ZhuyinScheme scheme;
} chewing_schemes [] = {
    {0, ZHUYIN_STANDARD},
    {1, ZHUYIN_GINYIEH},
    {2, ZHUYIN_ETEN},
    {3, ZHUYIN_IBM}
};

BopomofoConfig::BopomofoConfig ()
    : LibPinyinConfig ("com.github.libpinyin.ibus-libpinyin.libbopomofo")
{
}

void
BopomofoConfig::init ()
{
    if (m_instance.get () == NULL) {
        m_instance.reset (new BopomofoConfig ());
        m_instance->readDefaultValues ();
    }
    /*disable cloud input by default*/
    m_instance->disableCloudInput();
}

void
BopomofoConfig::readDefaultValues (void)
{
    LibPinyinConfig::readDefaultValues ();
#if !USE_G_SETTINGS_LIST_KEYS
    /* init states */
    m_init_chinese = read (CONFIG_INIT_CHINESE, true);
    m_init_full = read (CONFIG_INIT_FULL, false);
    m_init_full_punct = read (CONFIG_INIT_FULL_PUNCT, true);
    m_init_simp_chinese = read (CONFIG_INIT_SIMP_CHINESE, false);

    const gint map = read (CONFIG_BOPOMOFO_KEYBOARD_MAPPING, 0);
    m_bopomofo_keyboard_mapping = ZHUYIN_DEFAULT;

    for (guint i = 0; i < G_N_ELEMENTS (chewing_schemes); i++) {
        if (map == chewing_schemes[i].bopomofo_keyboard) {
            /* set chewing scheme. */
            m_bopomofo_keyboard_mapping = chewing_schemes[i].scheme;
        }
    }

    m_select_keys = read (CONFIG_SELECT_KEYS, 0);
    if (m_select_keys >= 9) m_select_keys = 0;
    m_guide_key = read (CONFIG_GUIDE_KEY, 1);
    m_auxiliary_select_key_f = read (CONFIG_AUXILIARY_SELECT_KEY_F, 1);
    m_auxiliary_select_key_kp = read (CONFIG_AUXILIARY_SELECT_KEY_KP, 1);
    m_enter_key = read (CONFIG_ENTER_KEY, true);
#endif
}

gboolean
BopomofoConfig::valueChanged (const std::string &schema_id,
                              const std::string &name,
                              GVariant          *value)
{
    if (m_schema_id != schema_id)
        return FALSE;

    if (LibPinyinConfig::valueChanged (schema_id, name, value))
        return TRUE;

    /* init states */
    if (CONFIG_INIT_CHINESE == name)
        m_init_chinese = normalizeGVariant (value, true);
    else if (CONFIG_INIT_FULL == name)
        m_init_full = normalizeGVariant (value, true);
    else if (CONFIG_INIT_FULL_PUNCT == name)
        m_init_full_punct = normalizeGVariant (value, true);
    else if (CONFIG_INIT_SIMP_CHINESE == name)
        m_init_simp_chinese = normalizeGVariant (value, false);
    else if (CONFIG_BOPOMOFO_KEYBOARD_MAPPING == name) {
        const gint map = normalizeGVariant (value, 0);
        m_bopomofo_keyboard_mapping = ZHUYIN_DEFAULT;

        for (guint i = 0; i < G_N_ELEMENTS (chewing_schemes); i++) {
            if (map == chewing_schemes[i].bopomofo_keyboard) {
                /* set chewing scheme. */
                m_bopomofo_keyboard_mapping = chewing_schemes[i].scheme;
            }
        }
    } else if (CONFIG_SELECT_KEYS == name) {
        m_select_keys = normalizeGVariant (value, 0);
        if (m_select_keys >= 9) m_select_keys = 0;
    }
    else if (CONFIG_GUIDE_KEY == name)
        m_guide_key = normalizeGVariant (value, true);
    else if (CONFIG_AUXILIARY_SELECT_KEY_F == name)
        m_auxiliary_select_key_f = normalizeGVariant (value, true);
    else if (CONFIG_AUXILIARY_SELECT_KEY_KP == name)
        m_auxiliary_select_key_kp = normalizeGVariant (value, true);
    else if (CONFIG_ENTER_KEY == name)
        m_enter_key = normalizeGVariant (value, true);
    else
        return FALSE;
    return TRUE;

}

};
