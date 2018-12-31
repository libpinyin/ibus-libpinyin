/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
 * Copyright (c) 2010 BYVoid <byvoid1@gmail.com>
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
#include "PYPBopomofoEngine.h"
#include <string>
#include <assert.h>
#include "PYPunctEditor.h"
#include "PYPBopomofoEditor.h"
#include "PYFallbackEditor.h"
#include "PYPSuggestionEditor.h"
#include "PYConfig.h"
#include "PYPConfig.h"

using namespace PY;

/* constructor */
BopomofoEngine::BopomofoEngine (IBusEngine *engine)
    : Engine (engine),
      m_props (BopomofoConfig::instance ()),
      m_prev_pressed_key (IBUS_VoidSymbol),
      m_input_mode (MODE_INIT),
      m_need_update (FALSE),
      m_fallback_editor (new FallbackEditor (m_props, BopomofoConfig::instance()))
{
    gint i;

    /* create editors */
    m_editors[MODE_INIT].reset (new BopomofoEditor (m_props, BopomofoConfig::instance ()));
    m_editors[MODE_PUNCT].reset (new PunctEditor (m_props, BopomofoConfig::instance ()));
    m_editors[MODE_SUGGESTION].reset (new SuggestionEditor (m_props, BopomofoConfig::instance ()));

    m_props.signalUpdateProperty ().connect
        (std::bind (&BopomofoEngine::updateProperty, this, _1));

    for (i = MODE_INIT; i < MODE_LAST; i++) {
        connectEditorSignals (m_editors[i]);
    }

    connectEditorSignals (m_fallback_editor);
}

/* destructor */
BopomofoEngine::~BopomofoEngine (void)
{
}

/* keep synced with pinyin engine. */
gboolean
BopomofoEngine::processAccelKeyEvent (guint keyval, guint keycode,
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
            if (BopomofoConfig::instance ().mainSwitch () == accel) {
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
    if (BopomofoConfig::instance (). letterSwitch () == accel) {
        m_props.toggleModeFull ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    /* Toggle full/half Punct Mode */
    if (BopomofoConfig::instance (). punctSwitch () == accel) {
        m_props.toggleModeFullPunct ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    /* Toggle simp/trad Chinese Mode */
    if (BopomofoConfig::instance ().tradSwitch () == accel) {
        m_props.toggleModeSimp ();
        m_prev_pressed_key = keyval;
        return TRUE;
    }

    return FALSE;
}

gboolean
BopomofoEngine::processKeyEvent (guint keyval, guint keycode, guint modifiers)
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
                /* m_editors[m_input_mode]->update (); */
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

        if (G_UNLIKELY (m_input_mode == MODE_INIT &&
                        m_editors[MODE_INIT]->text ().empty () &&
                        cmshm_filter (modifiers) == 0 &&
                        keyval == IBUS_grave)){
            /* if BopomofoEditor is empty and get a grave key,
             * switch current editor to PunctEditor */
            m_input_mode = MODE_PUNCT;
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
BopomofoEngine::focusIn (void)
{
    registerProperties (m_props.properties ());
}

void
BopomofoEngine::focusOut (void)
{
    Engine::focusOut ();

    reset ();
}

void
BopomofoEngine::reset (void)
{
    m_prev_pressed_key = IBUS_VoidSymbol;
    m_input_mode = MODE_INIT;
    for (gint i = 0; i < MODE_LAST; i++) {
        m_editors[i]->reset ();
    }
    m_fallback_editor->reset ();
}

void
BopomofoEngine::enable (void)
{
    m_props.reset ();
}

void
BopomofoEngine::disable (void)
{
}

void
BopomofoEngine::pageUp (void)
{
    m_editors[m_input_mode]->pageUp ();
}

void
BopomofoEngine::pageDown (void)
{
    m_editors[m_input_mode]->pageDown ();
}

void
BopomofoEngine::cursorUp (void)
{
    m_editors[m_input_mode]->cursorUp ();
}

void
BopomofoEngine::cursorDown (void)
{
    m_editors[m_input_mode]->cursorDown ();
}

inline void
BopomofoEngine::showSetupDialog (void)
{
    g_spawn_command_line_async
        (LIBEXECDIR"/ibus-setup-libpinyin libbopomofo", NULL);
}

gboolean
BopomofoEngine::propertyActivate (const gchar *prop_name,
                                           guint prop_state)
{
    const static std::string setup ("setup");
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
BopomofoEngine::candidateClicked (guint index,
                                           guint button,
                                           guint state)
{
    m_editors[m_input_mode]->candidateClicked (index, button, state);
}

void
BopomofoEngine::commitText (Text & text)
{
    Engine::commitText (text);

    if (m_input_mode != MODE_INIT && m_input_mode != MODE_SUGGESTION) {
        m_input_mode = MODE_INIT;
    } else if (BopomofoConfig::instance ().showSuggestion ()) {
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
BopomofoEngine::connectEditorSignals (EditorPtr editor)
{
    editor->signalCommitText ().connect (
        std::bind (&BopomofoEngine::commitText, this, _1));

    editor->signalUpdatePreeditText ().connect (
        std::bind (&BopomofoEngine::updatePreeditText, this, _1, _2, _3));
    editor->signalShowPreeditText ().connect (
        std::bind (&BopomofoEngine::showPreeditText, this));
    editor->signalHidePreeditText ().connect (
        std::bind (&BopomofoEngine::hidePreeditText, this));

    editor->signalUpdateAuxiliaryText ().connect (
        std::bind (&BopomofoEngine::updateAuxiliaryText, this, _1, _2));
    editor->signalShowAuxiliaryText ().connect (
        std::bind (&BopomofoEngine::showAuxiliaryText, this));
    editor->signalHideAuxiliaryText ().connect (
        std::bind (&BopomofoEngine::hideAuxiliaryText, this));

    editor->signalUpdateLookupTable ().connect (
        std::bind (&BopomofoEngine::updateLookupTable, this, _1, _2));
    editor->signalUpdateLookupTableFast ().connect (
        std::bind (&BopomofoEngine::updateLookupTableFast, this, _1, _2));
    editor->signalShowLookupTable ().connect (
        std::bind (&BopomofoEngine::showLookupTable, this));
    editor->signalHideLookupTable ().connect (
        std::bind (&BopomofoEngine::hideLookupTable, this));
}


