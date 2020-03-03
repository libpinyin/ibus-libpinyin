/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2011 Peng Wu <alexepico@gmail.com>
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

#include "PYLibPinyin.h"

#include <string.h>
#include <pinyin.h>
#include "PYPConfig.h"

#define LIBPINYIN_SAVE_TIMEOUT   (5 * 60)

using namespace PY;

std::unique_ptr<LibPinyinBackEnd> LibPinyinBackEnd::m_instance;

static LibPinyinBackEnd libpinyin_backend;

LibPinyinBackEnd::LibPinyinBackEnd () {
    m_timeout_id = 0;
    m_timer = g_timer_new ();
    m_pinyin_context = NULL;
    m_chewing_context = NULL;
}

LibPinyinBackEnd::~LibPinyinBackEnd () {
    g_timer_destroy (m_timer);
    if (m_timeout_id != 0) {
        saveUserDB ();
        g_source_remove (m_timeout_id);
    }

    if (m_pinyin_context)
        pinyin_fini(m_pinyin_context);
    m_pinyin_context = NULL;
    if (m_chewing_context)
        pinyin_fini(m_chewing_context);
    m_chewing_context = NULL;
}

pinyin_context_t *
LibPinyinBackEnd::initPinyinContext (Config *config)
{
    pinyin_context_t * context = NULL;

    gchar * userdir = g_build_filename (g_get_user_cache_dir (),
                                        "ibus", "libpinyin", NULL);
    int retval = g_mkdir_with_parents (userdir, 0700);
    if (retval) {
        g_free (userdir); userdir = NULL;
    }
    context = pinyin_init (LIBPINYIN_DATADIR, userdir);
    g_free (userdir);

    const char *dicts = config->dictionaries ().c_str ();
    gchar ** indices = g_strsplit_set (dicts, ";", -1);
    for (size_t i = 0; i < g_strv_length(indices); ++i) {
        int index = atoi (indices [i]);
        if (index <= 1)
            continue;

        pinyin_load_addon_phrase_library (context, index);
    }
    g_strfreev (indices);

    return context;
}

pinyin_instance_t *
LibPinyinBackEnd::allocPinyinInstance ()
{
    Config * config = &PinyinConfig::instance ();
    if (NULL == m_pinyin_context) {
        m_pinyin_context = initPinyinContext (config);
    }

    setPinyinOptions (config);
    return pinyin_alloc_instance (m_pinyin_context);
}

void
LibPinyinBackEnd::freePinyinInstance (pinyin_instance_t *instance)
{
    pinyin_free_instance (instance);
}

pinyin_context_t *
LibPinyinBackEnd::initChewingContext (Config *config)
{
    pinyin_context_t * context = NULL;

    gchar * userdir = g_build_filename (g_get_user_cache_dir (),
                                        "ibus", "libbopomofo", NULL);
    int retval = g_mkdir_with_parents (userdir, 0700);
    if (retval) {
        g_free(userdir); userdir = NULL;
    }
    context = pinyin_init (LIBPINYIN_DATADIR, userdir);
    g_free(userdir);

    const char *dicts = config->dictionaries ().c_str ();
    gchar ** indices = g_strsplit_set (dicts, ";", -1);
    for (size_t i = 0; i < g_strv_length(indices); ++i) {
        int index = atoi (indices [i]);
        if (index <= 1)
            continue;

        pinyin_load_addon_phrase_library (context, index);
    }
    g_strfreev (indices);

    return context;
}

pinyin_instance_t *
LibPinyinBackEnd::allocChewingInstance ()
{
    Config *config = &BopomofoConfig::instance ();
    if (NULL == m_chewing_context) {
        m_chewing_context = initChewingContext (config);
    }

    setChewingOptions (config);
    return pinyin_alloc_instance (m_chewing_context);
}

void
LibPinyinBackEnd::freeChewingInstance (pinyin_instance_t *instance)
{
    pinyin_free_instance (instance);
}

void
LibPinyinBackEnd::init (void) {
    g_assert (NULL == m_instance.get ());
    LibPinyinBackEnd * backend = new LibPinyinBackEnd;
    m_instance.reset (backend);
}

void
LibPinyinBackEnd::finalize (void) {
    m_instance.reset ();
}

gboolean
LibPinyinBackEnd::setPinyinOptions (Config *config)
{
    if (NULL == m_pinyin_context)
        return FALSE;

    DoublePinyinScheme scheme = config->doublePinyinSchema ();
    pinyin_set_double_pinyin_scheme (m_pinyin_context, scheme);

    pinyin_option_t options = config->option()
        | USE_RESPLIT_TABLE | USE_DIVIDED_TABLE;
    pinyin_set_options (m_pinyin_context, options);
    return TRUE;
}

gboolean
LibPinyinBackEnd::setChewingOptions (Config *config)
{
    if (NULL == m_chewing_context)
        return FALSE;

    ZhuyinScheme scheme = config->bopomofoKeyboardMapping ();
    pinyin_set_zhuyin_scheme (m_chewing_context, scheme);

    pinyin_option_t options = config->option() | USE_TONE;
    pinyin_set_options(m_chewing_context, options);
    return TRUE;
}

void
LibPinyinBackEnd::modified (void)
{
    /* Restart the timer */
    g_timer_start (m_timer);

    if (m_timeout_id != 0)
        return;

    m_timeout_id = g_timeout_add_seconds (LIBPINYIN_SAVE_TIMEOUT,
                                          LibPinyinBackEnd::timeoutCallback,
                                          static_cast<gpointer> (this));
}

gboolean
LibPinyinBackEnd::importPinyinDictionary (const char *filename)
{
    /* user phrase library should be already loaded here. */
    FILE * dictfile = fopen (filename, "r");
    if (NULL == dictfile)
        return FALSE;

    import_iterator_t * iter = pinyin_begin_add_phrases
        (m_pinyin_context, USER_DICTIONARY);

    if (NULL == iter) {
        fclose(dictfile);
        return FALSE;
    }

    char* linebuf = NULL; size_t size = 0; ssize_t read;
    while ((read = getline (&linebuf, &size, dictfile)) != -1) {
        if (0 == strlen (linebuf))
            continue;

        if ( '\n' == linebuf[strlen (linebuf) - 1] ) {
            linebuf[strlen (linebuf) - 1] = '\0';
        }

        gchar ** items = g_strsplit_set (linebuf, " \t", 3);
        guint len = g_strv_length (items);

        gchar * phrase = NULL, * pinyin = NULL;
        gint count = -1;
        if (2 == len || 3 == len) {
            phrase = items[0];
            pinyin = items[1];
            if (3 == len)
                count = atoi (items[2]);
        } else
            continue;

        pinyin_iterator_add_phrase (iter, phrase, pinyin, count);

        g_strfreev (items);
    }

    pinyin_end_add_phrases (iter);
    fclose (dictfile);

    pinyin_save (m_pinyin_context);
    return TRUE;
}

gboolean
LibPinyinBackEnd::exportPinyinDictionary (const char *filename)
{
    /* user phrase library should be already loaded here. */
    FILE * dictfile = fopen (filename, "w");
    if (NULL == dictfile)
        return FALSE;

    export_iterator_t * iter = pinyin_begin_get_phrases
        (m_pinyin_context, USER_DICTIONARY);

    if (NULL == iter) {
        fclose(dictfile);
        return FALSE;
    }

    /* use " " as the separator. */
    while (pinyin_iterator_has_next_phrase (iter)) {
        gchar * phrase = NULL; gchar * pinyin = NULL;
        gint count = -1;

        g_assert (pinyin_iterator_get_next_phrase (iter, &phrase, &pinyin, &count));

        if (-1 == count) /* skip output the default count. */
            fprintf (dictfile, "%s %s\n", phrase, pinyin);
        else /* output the count. */
            fprintf (dictfile, "%s %s %d\n", phrase, pinyin, count);

        g_free (phrase); g_free (pinyin);
    }

    pinyin_end_get_phrases(iter);
    fclose (dictfile);
    return TRUE;
}

gboolean
LibPinyinBackEnd::clearPinyinUserData (const char *target)
{
    if (NULL == m_pinyin_context)
        return FALSE;

    if (0 == strcmp ("all", target)) {
        pinyin_mask_out (m_pinyin_context, 0x0, 0x0);
    } else if (0 == strcmp ("user", target)) {
        /* clear addon dictionary. */
        pinyin_mask_out (m_pinyin_context, PHRASE_INDEX_LIBRARY_MASK,
                         PHRASE_INDEX_MAKE_TOKEN (ADDON_DICTIONARY, null_token));
        /* clear user dictionary. */
        pinyin_mask_out (m_pinyin_context, PHRASE_INDEX_LIBRARY_MASK,
                        PHRASE_INDEX_MAKE_TOKEN (USER_DICTIONARY, null_token));
    } else {
        g_warning ("unknown clear target: %s.\n", target);
    }

    pinyin_save (m_pinyin_context);
    return TRUE;
}

gboolean
LibPinyinBackEnd::rememberUserInput (pinyin_instance_t *instance,
                                     const gchar *phrase)
{
    /* pre-check the incomplete pinyin keys, prepare pinyin string,
       remember user input. */
    pinyin_remember_user_input (instance, phrase, -1);

    /* save later,
       will mark modified from pinyin/bopomofo editor. */
    return TRUE;
}

gboolean
LibPinyinBackEnd::timeoutCallback (gpointer data)
{
    LibPinyinBackEnd *self = static_cast<LibPinyinBackEnd *> (data);

    /* Get the elapsed time since last modification of database. */
    guint elapsed = (guint)g_timer_elapsed (self->m_timer, NULL);

    if (elapsed >= LIBPINYIN_SAVE_TIMEOUT &&
        self->saveUserDB ()) {
        self->m_timeout_id = 0;
        return FALSE;
    }

    return TRUE;
}

gboolean
LibPinyinBackEnd::saveUserDB (void)
{
    if (m_pinyin_context)
        pinyin_save (m_pinyin_context);
    if (m_chewing_context)
        pinyin_save (m_chewing_context);
    return TRUE;
}
