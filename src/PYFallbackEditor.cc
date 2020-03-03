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
#include "PYFallbackEditor.h"
#include "PYHalfFullConverter.h"
#include "PYPinyinProperties.h"

namespace PY {

inline gboolean
FallbackEditor::processPunctForSimplifiedChinese (guint keyval, guint keycode, guint modifiers)
{
    switch (keyval) {
    case '`':
        commit ("·"); return TRUE;
    case '~':
        commit ("～"); return TRUE;
    case '!':
        commit ("！"); return TRUE;
    // case '@':
    // case '#':
    case '$':
        commit ("￥"); return TRUE;
    // case '%':
    case '^':
        commit ("……"); return TRUE;
    // case '&':
    // case '*':
    case '(':
        commit ("（"); return TRUE;
    case ')':
        commit ("）"); return TRUE;
    // case '-':
    case '_':
        commit ("——"); return TRUE;
    // case '=':
    // case '+':
    case '[':
        commit ("【"); return TRUE;
    case ']':
        commit ("】"); return TRUE;
    case '{':
        commit ("『"); return TRUE;
    case '}':
        commit ("』"); return TRUE;
    case '\\':
        commit ("、"); return TRUE;
    // case '|':
    case ';':
        commit ("；"); return TRUE;
    case ':':
        commit ("："); return TRUE;
    case '\'':
        commit (m_quote ? "‘" : "’");
        m_quote = !m_quote;
        return TRUE;
    case '"':
        commit (m_double_quote ? "“" : "”");
        m_double_quote = !m_double_quote;
        return TRUE;
    case ',':
        if (m_prev_committed_char >= '0' && m_prev_committed_char <= '9') {
            m_prev_committed_char = keyval;
            return FALSE;
        } else {
            commit ("，");
            return TRUE;
        }
    case '.':
        if (m_prev_committed_char >= '0' && m_prev_committed_char <= '9') {
            m_prev_committed_char = keyval;
            return FALSE;
        } else {
            commit ("。");
            return TRUE;
        }
    case '<':
        commit ("《"); return TRUE;
    case '>':
        commit ("》"); return TRUE;
    // case '/':
    case '?':
        commit ("？"); return TRUE;
    }
    return FALSE;
}

inline gboolean
FallbackEditor::processPunctForTraditionalChinese (guint keyval, guint keycode, guint modifiers)
{
    switch (keyval) {
    case '~':
        commit ("～"); return TRUE;
    case '!':
        commit ("！"); return TRUE;
    // case '@':
    // case '#':
    case '$':
        commit ("￥"); return TRUE;
    // case '%':
    case '^':
        commit ("……"); return TRUE;
    // case '&':
    // case '*':
    case '(':
        commit ("（"); return TRUE;
    case ')':
        commit ("）"); return TRUE;
    // case '-':
    case '_':
        commit ("——"); return TRUE;
    // case '=':
    // case '+':
    case '[':
        commit ("「"); return TRUE;
    case ']':
        commit ("」"); return TRUE;
    case '{':
        commit ("『"); return TRUE;
    case '}':
        commit ("』"); return TRUE;
    case '\\':
        commit ("、"); return TRUE;
    // case '|':
    case ';':
        commit ("；"); return TRUE;
    case ':':
        commit ("："); return TRUE;
    case '\'':
        commit (m_quote ? "‘" : "’");
        m_quote = !m_quote;
        return TRUE;
    case '"':
        commit (m_double_quote ? "“" : "”");
        m_double_quote = !m_double_quote;
        return TRUE;
    case ',':
        if (m_prev_committed_char >= '0' && m_prev_committed_char <= '9') {
            m_prev_committed_char = keyval;
            return FALSE;
        } else {
            commit ("，");
            return TRUE;
        }
    case '.':
        if (m_prev_committed_char >= '0' && m_prev_committed_char <= '9') {
            m_prev_committed_char = keyval;
            return FALSE;
        } else {
            commit ("。");
            return TRUE;
        }
    case '<':
        commit ("《"); return TRUE;
    case '>':
        commit ("》"); return TRUE;
    case '?':
        commit ("？"); return TRUE;
    }
    return FALSE;
}

inline gboolean
FallbackEditor::processPunct (guint keyval, guint keycode, guint modifiers)
{
    guint cmshm_modifiers = cmshm_filter (modifiers);

    /* check ctrl, alt, hyper, supper masks */
    if (cmshm_modifiers != 0)
        return FALSE;

    /* English mode */
    if (G_UNLIKELY (!m_props.modeChinese ())) {
        if (G_UNLIKELY (m_props.modeFull ()))
            commit (HalfFullConverter::toFull (keyval));
        else
            return FALSE;
        return TRUE;
    }
    else {
        /* Chinese mode */
        if (m_props.modeFullPunct ()) {
            if (m_props.modeSimp ()) {
                if (processPunctForSimplifiedChinese (keyval, keycode, modifiers))
                    return TRUE;
            }
            else {
                if (processPunctForTraditionalChinese (keyval, keycode, modifiers))
                    return TRUE;
            }
        }
        if (m_props.modeFull ())
            commit (HalfFullConverter::toFull (keyval));
        else
            return FALSE;
    }
    return TRUE;
}

gboolean
FallbackEditor::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    gboolean retval = FALSE;

    modifiers &= (IBUS_CONTROL_MASK |
                  IBUS_MOD1_MASK |
                  IBUS_SUPER_MASK |
                  IBUS_HYPER_MASK |
                  IBUS_META_MASK);

    switch (keyval) {
        /* numbers */
        case IBUS_KP_0 ... IBUS_KP_9:
            keyval = keyval - IBUS_KP_0 + IBUS_0;
        case IBUS_0 ... IBUS_9:
        /* letters */
        case IBUS_a ... IBUS_z:
        case IBUS_A ... IBUS_Z:
            if (modifiers == 0) {
                if (!m_props.modeFull ()) {
                    m_prev_committed_char = keyval;
                    return FALSE;
                }

                commit (HalfFullConverter::toFull (keyval));
                retval = TRUE;
            }
            break;
        /* punct */
        case IBUS_exclam ... IBUS_slash:
        case IBUS_colon ... IBUS_at:
        case IBUS_bracketleft ... IBUS_quoteleft:
        case IBUS_braceleft ... IBUS_asciitilde:
            retval = processPunct (keyval, keycode, modifiers);
            break;
        case IBUS_KP_Equal:
            retval = processPunct ('=', keycode, modifiers);
            break;
        case IBUS_KP_Multiply:
            retval = processPunct ('*', keycode, modifiers);
            break;
        case IBUS_KP_Add:
            retval = processPunct ('+', keycode, modifiers);
            break;
        #if 0
        case IBUS_KP_Separator:
            retval = processPunct (IBUS_separator, keycode, modifiers);
            break;
        #endif
        case IBUS_KP_Subtract:
            retval = processPunct ('-', keycode, modifiers);
            break;
        case IBUS_KP_Decimal:
            retval = FALSE;
            break;
        case IBUS_KP_Divide:
            retval = processPunct ('/', keycode, modifiers);
            break;
        /* space */
        case IBUS_KP_Space:
            keyval = IBUS_space;
        case IBUS_space:
            if (modifiers == 0) {
                if (!m_props.modeFull ()) {
                    m_prev_committed_char = keyval;
                    return FALSE;
                }

                commit ("　");
                retval = TRUE;
            }
            break;
        /* others */
        default:
            break;
    }

    if (!retval)
        m_prev_committed_char = keyval;
    return retval;
}

void
FallbackEditor::reset (void) {
    m_quote = TRUE;
    m_double_quote = TRUE;
    m_prev_committed_char = 0;
}

};
