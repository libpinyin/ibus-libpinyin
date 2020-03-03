/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
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
#include "PYPPinyinEngine.h"
#include <string>
#include <assert.h>
#include "PYConfig.h"
#include "PYPConfig.h"
#include "PYPunctEditor.h"
#include "PYRawEditor.h"
#ifdef IBUS_BUILD_LUA_EXTENSION
#include "PYExtEditor.h"
#endif
#ifdef IBUS_BUILD_ENGLISH_INPUT_MODE
#include "PYEnglishEditor.h"
#endif
#ifdef IBUS_BUILD_STROKE_INPUT_MODE
#include "PYStrokeEditor.h"
#endif
#include "PYPFullPinyinEditor.h"
#include "PYPDoublePinyinEditor.h"
#include "PYFallbackEditor.h"
#include "PYPSuggestionEditor.h"

using namespace PY;

/* constructor */
PinyinEngine::PinyinEngine (IBusEngine *engine)
    : Engine (engine),
      m_props (PinyinConfig::instance ()),
      m_prev_pressed_key (IBUS_VoidSymbol),
      m_input_mode (MODE_INIT),
      m_need_update (FALSE),
      m_fallback_editor (new FallbackEditor (m_props, PinyinConfig::instance ()))
{
    gint i;

#ifdef IBUS_BUILD_LUA_EXTENSION
    initLuaPlugin ();
#endif

    m_double_pinyin = PinyinConfig::instance ().doublePinyin ();

    if (m_double_pinyin) {
        DoublePinyinEditor *editor = new DoublePinyinEditor
            (m_props, PinyinConfig::instance ());
        m_editors[MODE_INIT].reset (editor);
#ifdef IBUS_BUILD_LUA_EXTENSION
        editor->setLuaPlugin (m_lua_plugin);
#endif
    } else {
        FullPinyinEditor *editor = new FullPinyinEditor
            (m_props, PinyinConfig::instance ());
        m_editors[MODE_INIT].reset (editor);
#ifdef IBUS_BUILD_LUA_EXTENSION
        editor->setLuaPlugin (m_lua_plugin);
#endif
    }

    m_editors[MODE_PUNCT].reset
        (new PunctEditor (m_props, PinyinConfig::instance ()));
    m_editors[MODE_RAW].reset
        (new RawEditor (m_props, PinyinConfig::instance ()));

#ifdef IBUS_BUILD_LUA_EXTENSION
    {
        ExtEditor *editor = new ExtEditor (m_props, PinyinConfig::instance ());
        m_editors[MODE_EXTENSION].reset (editor);
        editor->setLuaPlugin (m_lua_plugin);
    }
#else
    m_editors[MODE_EXTENSION].reset (new Editor (m_props, PinyinConfig::instance ()));
#endif
#ifdef IBUS_BUILD_ENGLISH_INPUT_MODE
    m_editors[MODE_ENGLISH].reset (new EnglishEditor (m_props, PinyinConfig::instance ()));
#else
    m_editors[MODE_ENGLISH].reset (new Editor (m_props, PinyinConfig::instance ()));
#endif
#ifdef IBUS_BUILD_STROKE_INPUT_MODE
    m_editors[MODE_STROKE].reset (new StrokeEditor (m_props, PinyinConfig::instance ()));
#else
    m_editors[MODE_STROKE].reset (new Editor (m_props, PinyinConfig::instance ()));
#endif

    {
        SuggestionEditor *editor = new SuggestionEditor
            (m_props, PinyinConfig::instance ());
        m_editors[MODE_SUGGESTION].reset (editor);
#ifdef IBUS_BUILD_LUA_EXTENSION
        editor->setLuaPlugin (m_lua_plugin);
#endif
    }

    m_props.signalUpdateProperty ().connect
        (std::bind (&PinyinEngine::updateProperty, this, _1));

    for (i = MODE_INIT; i < MODE_LAST; i++) {
        connectEditorSignals (m_editors[i]);
    }

    connectEditorSignals (m_fallback_editor);
}

/* destructor */
PinyinEngine::~PinyinEngine (void)
{
}

#ifdef IBUS_BUILD_LUA_EXTENSION
gboolean
PinyinEngine::initLuaPlugin (void)
{
    m_lua_plugin = ibus_engine_plugin_new ();

    loadLuaScript ( ".." G_DIR_SEPARATOR_S "lua" G_DIR_SEPARATOR_S "base.lua")||
        loadLuaScript (PKGDATADIR G_DIR_SEPARATOR_S "base.lua");

    gchar * path = g_build_filename (g_get_user_config_dir (),
                             "ibus", "libpinyin", "user.lua", NULL);
    loadLuaScript(path);
    g_free(path);

    return TRUE;
}

gboolean
PinyinEngine::loadLuaScript (const char * filename)
{
    return !ibus_engine_plugin_load_lua_script (m_lua_plugin, filename);
}
#endif

/* keep synced with bopomofo engine. */
gboolean
PinyinEngine::processAccelKeyEvent (guint keyval, guint keycode,
                                    guint modifiers)
{
    std::string accel;
    pinyin_accelerator_name (keyval, modifiers, accel);

    /* Safe Guard for empty key. */
    if ("" == accel)
        return FALSE;

    /* check Shift or Ctrl + Release hotkey,
     * and then ignore other Release key event */
    if (modifiers & IBUS_RELEASE_MASK) {
        /* press and release keyval are same,
         * and no other key event between the press and release key event */
        gboolean triggered = FALSE;

        if (m_prev_pressed_key == keyval) {
            if (PinyinConfig::instance ().mainSwitch () == accel) {
                triggered = TRUE;
            }
        }

        if (triggered) {
            if (!m_editors[MODE_INIT]->text ().empty ()) {
                Text text (m_editors[MODE_INIT]->text ());
                commitText (text);
                m_editors[MODE_INIT]->reset ();
            }

            if (!m_editors[MODE_SUGGESTION]->text ().empty ())
                m_editors[MODE_SUGGESTION]->reset ();
            m_props.toggleModeChinese ();
            return FALSE;
        }

        if (m_input_mode == MODE_INIT &&
            m_editors[MODE_INIT]->text ().empty ()) {
            /* If it is in init mode, and no any previous input text,
             * we will let client applications to handle release key event */
            return FALSE;
        } else {
            return TRUE;
        }
    }

    /* Toggle full/half Letter Mode */
    if (PinyinConfig::instance ().letterSwitch () == accel) {
        m_props.toggleModeFull ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    /* Toggle full/half Punct Mode */
    if (PinyinConfig::instance ().punctSwitch () == accel) {
        m_props.toggleModeFullPunct ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    /* Toggle both full/half Mode */
    if (PinyinConfig::instance ().bothSwitch () == accel) {
        if (m_props.modeFull () != m_props.modeFullPunct ()) {
            m_props.toggleModeFull ();
            m_prev_pressed_key = keyval;
            return TRUE;
        }

        m_props.toggleModeFull ();
        m_props.toggleModeFullPunct ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    /* Toggle simp/trad Chinese Mode */
    if (PinyinConfig::instance ().tradSwitch () == accel) {
        m_props.toggleModeSimp ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    return FALSE;
}

gboolean
PinyinEngine::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    gboolean retval = FALSE;

    if (contentIsPassword ())
        return retval;

    if (processAccelKeyEvent (keyval, keycode, modifiers))
        return TRUE;

    /* assume release key event is handled in processAccelKeyEvent. */
    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;

    if (m_props.modeChinese ()) {
        /* return from MODE_SUGGESTION to normal input. */
        if (m_input_mode == MODE_SUGGESTION) {
            /* only accept input to select candidate. */
            if (IBUS_Escape == keyval) {
                m_editors[m_input_mode]->reset ();
                m_input_mode = MODE_INIT;
                m_editors[m_input_mode]->reset ();
                /* m_editors[m_input_mode]->update ();*/
                return TRUE;
            }

            retval = m_editors[m_input_mode]->processKeyEvent (keyval, keycode, modifiers);

            if (retval) {
                goto out;
            } else {
                m_editors[m_input_mode]->reset ();
                m_input_mode = MODE_INIT;
            }
        }

        /* handle normal input. */
        if (m_input_mode == MODE_INIT &&
            (cmshm_filter (modifiers) == 0)) {
            const String & text = m_editors[MODE_INIT]->text ();
            if (text.empty ()) {
                switch (keyval) {
                case IBUS_grave:
                    if (m_props.modeFullPunct ())
                        m_input_mode = MODE_PUNCT;
                    break;
#ifdef IBUS_BUILD_LUA_EXTENSION
                case IBUS_i:
                    // for full pinyin
                    if (PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_EXTENSION;
                    break;
                case IBUS_I:
                    // for double pinyin
                    if (!PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_EXTENSION;
                    break;
#endif
#ifdef IBUS_BUILD_ENGLISH_INPUT_MODE
                case IBUS_v:
                    // for full pinyin
                    if (PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_ENGLISH;
                    break;
                case IBUS_V:
                    // for double pinyin
                    if (!PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_ENGLISH;
                    break;
#endif
#ifdef IBUS_BUILD_STROKE_INPUT_MODE
                case IBUS_u:
                    // for full pinyin
                    if (PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_STROKE;
                    break;
                case IBUS_U:
                    // for double pinyin
                    if (!PinyinConfig::instance ().doublePinyin ())
                        break;
                    m_input_mode = MODE_STROKE;
                    break;
#endif
                }
            } else {
                /* TODO: Unknown */
            }
        }
        retval = m_editors[m_input_mode]->processKeyEvent (keyval, keycode, modifiers);
        if (G_UNLIKELY (retval &&
                        m_input_mode != MODE_INIT &&
                        m_editors[m_input_mode]->text ().empty ()))
            m_input_mode = MODE_INIT;
    }

    if (G_UNLIKELY (!retval))
        retval = m_fallback_editor->processKeyEvent (keyval, keycode, modifiers);

out:
    /* needed for SuggestionEditor */
    if (m_need_update) {
        m_editors[m_input_mode]->update ();
        m_need_update = FALSE;
    }
    /* store ignored key event by editors */
    m_prev_pressed_key = retval ? IBUS_VoidSymbol : keyval;

    return retval;
}

void
PinyinEngine::focusIn (void)
{
    /* TODO: check memory leak here,
     *       or switch full/double pinyin when pinyin config is changed.*/
    if (PinyinConfig::instance ().doublePinyin ()) {
        if (!m_double_pinyin) {
            DoublePinyinEditor *editor = new DoublePinyinEditor
                (m_props, PinyinConfig::instance ());
            m_editors[MODE_INIT].reset (editor);
#ifdef IBUS_BUILD_LUA_EXTENSION
            editor->setLuaPlugin (m_lua_plugin);
#endif
            connectEditorSignals (m_editors[MODE_INIT]);
        }
        m_double_pinyin = TRUE;
    }
    else {
        if (m_double_pinyin) {
            FullPinyinEditor *editor = new FullPinyinEditor
                (m_props, PinyinConfig::instance ());
            m_editors[MODE_INIT].reset (editor);
#ifdef IBUS_BUILD_LUA_EXTENSION
            editor->setLuaPlugin (m_lua_plugin);
#endif
            connectEditorSignals (m_editors[MODE_INIT]);
        }
        m_double_pinyin = FALSE;
    }

    registerProperties (m_props.properties ());
}

void
PinyinEngine::focusOut (void)
{
    Engine::focusOut ();

    reset ();
}

void
PinyinEngine::reset (void)
{
    m_prev_pressed_key = IBUS_VoidSymbol;
    m_input_mode = MODE_INIT;
    for (gint i = 0; i < MODE_LAST; i++) {
        m_editors[i]->reset ();
    }
    m_fallback_editor->reset ();
}

void
PinyinEngine::enable (void)
{
    m_props.reset ();
}

void
PinyinEngine::disable (void)
{
}

void
PinyinEngine::pageUp (void)
{
    m_editors[m_input_mode]->pageUp ();
}

void
PinyinEngine::pageDown (void)
{
    m_editors[m_input_mode]->pageDown ();
}

void
PinyinEngine::cursorUp (void)
{
    m_editors[m_input_mode]->cursorUp ();
}

void
PinyinEngine::cursorDown (void)
{
    m_editors[m_input_mode]->cursorDown ();
}

inline void
PinyinEngine::showSetupDialog (void)
{
    g_spawn_command_line_async
        (LIBEXECDIR"/ibus-setup-libpinyin libpinyin", NULL);
}

gboolean
PinyinEngine::propertyActivate (const char *prop_name, guint prop_state)
{
    const static String setup ("setup");
    if (m_props.propertyActivate (prop_name, prop_state)) {
        return TRUE;
    }
    else if (setup == prop_name) {
        showSetupDialog ();
        return TRUE;
    }
    return FALSE;
}

void
PinyinEngine::candidateClicked (guint index, guint button, guint state)
{
    m_editors[m_input_mode]->candidateClicked (index, button, state);
}

void
PinyinEngine::commitText (Text & text)
{
    Engine::commitText (text);

    if (m_input_mode != MODE_INIT && m_input_mode != MODE_SUGGESTION) {
        m_input_mode = MODE_INIT;
    } else if (PinyinConfig::instance ().showSuggestion ()) {
        m_input_mode = MODE_SUGGESTION;
        m_editors[m_input_mode]->setText (text.text (), 0);
        m_need_update = TRUE;
    } else {
        m_input_mode = MODE_INIT;
    }

#if 1
    /* handle "<num>+.<num>+" here */
    if (text.text ())
        static_cast<FallbackEditor*> (m_fallback_editor.get ())->setPrevCommittedChar (*text.text ());
    else
        static_cast<FallbackEditor*> (m_fallback_editor.get ())->setPrevCommittedChar (0);
#endif
}

void
PinyinEngine::connectEditorSignals (EditorPtr editor)
{
    editor->signalCommitText ().connect (
        std::bind (&PinyinEngine::commitText, this, _1));

    editor->signalUpdatePreeditText ().connect (
        std::bind (&PinyinEngine::updatePreeditText, this, _1, _2, _3));
    editor->signalShowPreeditText ().connect (
        std::bind (&PinyinEngine::showPreeditText, this));
    editor->signalHidePreeditText ().connect (
        std::bind (&PinyinEngine::hidePreeditText, this));

    editor->signalUpdateAuxiliaryText ().connect (
        std::bind (&PinyinEngine::updateAuxiliaryText, this, _1, _2));
    editor->signalShowAuxiliaryText ().connect (
        std::bind (&PinyinEngine::showAuxiliaryText, this));
    editor->signalHideAuxiliaryText ().connect (
        std::bind (&PinyinEngine::hideAuxiliaryText, this));

    editor->signalUpdateLookupTable ().connect (
        std::bind (&PinyinEngine::updateLookupTable, this, _1, _2));
    editor->signalUpdateLookupTableFast ().connect (
        std::bind (&PinyinEngine::updateLookupTableFast, this, _1, _2));
    editor->signalShowLookupTable ().connect (
        std::bind (&PinyinEngine::showLookupTable, this));
    editor->signalHideLookupTable ().connect (
        std::bind (&PinyinEngine::hideLookupTable, this));
}
