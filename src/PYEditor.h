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
#ifndef __PY_EDITOR_H_
#define __PY_EDITOR_H_

#include <glib.h>
#include "PYSignal.h"
#include "PYString.h"
#include "PYUtil.h"
#include "PYPEnhancedCandidates.h"

namespace PY {

class Text;
class LookupTable;
class PinyinProperties;
class Config;

class Editor;
typedef std::shared_ptr<Editor> EditorPtr;

class TraditionalCandidates;
class LuaTriggerCandidates;
class LuaConverterCandidates;
class EmojiCandidates;

class Editor {
    friend class TraditionalCandidates;

#ifdef IBUS_BUILD_LUA_EXTENSION
    friend class LuaTriggerCandidates;
    friend class LuaConverterCandidates;
#endif

    friend class EmojiCandidates;

public:
    Editor (PinyinProperties & prop, Config & config);
    virtual ~Editor (void);

    virtual gboolean processKeyEvent (guint keyval, guint keycode, guint modifiers);
    virtual void pageUp (void);
    virtual void pageDown (void);
    virtual void cursorUp (void);
    virtual void cursorDown (void);
    virtual void update (void);
    virtual void reset (void);
    virtual void candidateClicked (guint index, guint button, guint state);

    const String & text (void) const
    {
        return m_text;
    }

    void setText (const String & text, guint cursor)
    {
        m_text = text;
        m_cursor = cursor;
    }

    /* signals */
    signal <void (Text &)> & signalCommitText (void)    { return m_signal_commit_text; }

    signal <void (Text &, guint, gboolean)> & signalUpdatePreeditText (void)
                                                        { return m_signal_update_preedit_text; }
    signal <void ()> & signalShowPreeditText (void)     { return m_signal_show_preedit_text; }
    signal <void ()> & signalHidePreeditText (void)     { return m_signal_hide_preedit_text; }

    signal <void (Text &, gboolean)> & signalUpdateAuxiliaryText (void)
                                                        { return m_signal_update_auxiliary_text; }
    signal <void ()> & signalShowAuxiliaryText (void)   { return m_signal_show_auxiliary_text; }
    signal <void ()> & signalHideAuxiliaryText (void)   { return m_signal_hide_auxiliary_text; }

    signal <void (LookupTable &, gboolean)> & signalUpdateLookupTable (void)
                                                        { return m_signal_update_lookup_table; }
    signal <void (LookupTable &, gboolean)> & signalUpdateLookupTableFast (void)
                                                        { return m_signal_update_lookup_table_fast; }
    signal <void ()> & signalShowLookupTable (void)     { return m_signal_show_lookup_table; }
    signal <void ()> & signalHideLookupTable (void)     { return m_signal_hide_lookup_table; }

protected:
    /* methods */
    void commitText (Text & text) const
    {
        m_signal_commit_text (text);
    }

    void updatePreeditText (Text & text, guint cursor, gboolean visible) const
    {
        m_signal_update_preedit_text (text, cursor, visible);
    }

    void showPreeditText (void) const
    {
        m_signal_show_preedit_text ();
    }

    void hidePreeditText (void) const
    {
        m_signal_hide_preedit_text ();
    }

    void updateAuxiliaryText (Text & text, gboolean visible) const
    {
        m_signal_update_auxiliary_text (text, visible);
    }

    void showAuxiliaryText (void) const
    {
        m_signal_show_auxiliary_text ();
    }

    void hideAuxiliaryText (void) const
    {
        m_signal_hide_auxiliary_text ();
    }

    void updateLookupTable (LookupTable & table, gboolean visible) const
    {
        m_signal_update_lookup_table (table, visible);
    }

    void updateLookupTableFast (LookupTable & table, gboolean visible) const
    {
        m_signal_update_lookup_table_fast (table, visible);
    }

    void showLookupTable (void) const
    {
        m_signal_show_lookup_table ();
    }

    void hideLookupTable (void) const
    {
        m_signal_hide_lookup_table ();
    }

protected:
    virtual int selectCandidateInternal (EnhancedCandidate & candidate);
    virtual gboolean removeCandidateInternal (EnhancedCandidate & candidate);

    /* signals */
    signal <void (Text &)> m_signal_commit_text;
    signal <void ( Text &, guint, gboolean)> m_signal_update_preedit_text;
    signal <void ()> m_signal_show_preedit_text;
    signal <void ()> m_signal_hide_preedit_text;
    signal <void (Text &, gboolean)> m_signal_update_auxiliary_text;
    signal <void ()> m_signal_show_auxiliary_text;
    signal <void ()> m_signal_hide_auxiliary_text;
    signal <void (LookupTable &, gboolean)> m_signal_update_lookup_table;
    signal <void (LookupTable &, gboolean)> m_signal_update_lookup_table_fast;
    signal <void ()> m_signal_show_lookup_table;
    signal <void ()> m_signal_hide_lookup_table;

protected:
    String m_text;
    guint  m_cursor;
    PinyinProperties & m_props;
    Config & m_config;
};

};

#endif
