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

#include "PYEngine.h"
#include <cstring>
#include "PYPPinyinEngine.h"
#include "PYPBopomofoEngine.h"

namespace PY {
/* code of engine class of GObject */
#define IBUS_PINYIN_ENGINE(obj)             \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), IBUS_TYPE_PINYIN_ENGINE, IBusPinyinEngine))
#define IBUS_PINYIN_ENGINE_CLASS(klass)     \
    (G_TYPE_CHECK_CLASS_CAST ((klass), IBUS_TYPE_PINYIN_ENGINE, IBusPinyinEngineClass))
#define IBUS_IS_PINYIN_ENGINE(obj)          \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), IBUS_TYPE_PINYIN_ENGINE))
#define IBUS_IS_PINYIN_ENGINE_CLASS(klass)  \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), IBUS_TYPE_PINYIN_ENGINE))
#define IBUS_PINYIN_ENGINE_GET_CLASS(obj)   \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), IBUS_TYPE_PINYIN_ENGINE, IBusPinyinEngineClass))


typedef struct _IBusPinyinEngine IBusPinyinEngine;
typedef struct _IBusPinyinEngineClass IBusPinyinEngineClass;

struct _IBusPinyinEngine {
    IBusEngine parent;

    /* members */
    Engine *engine;
};

struct _IBusPinyinEngineClass {
    IBusEngineClass parent;
};

/* functions prototype */
static void     ibus_pinyin_engine_class_init   (IBusPinyinEngineClass  *klass);
static void     ibus_pinyin_engine_init         (IBusPinyinEngine       *pinyin);
static GObject* ibus_pinyin_engine_constructor  (GType                   type,
                                                 guint                   n_construct_params,
                                                 GObjectConstructParam  *construct_params);

static void     ibus_pinyin_engine_destroy      (IBusPinyinEngine       *pinyin);
static gboolean ibus_pinyin_engine_process_key_event
                                                (IBusEngine             *engine,
                                                 guint                   keyval,
                                                 guint                   keycode,
                                                 guint                   modifiers);
static void     ibus_pinyin_engine_focus_in     (IBusEngine             *engine);
static void     ibus_pinyin_engine_focus_out    (IBusEngine             *engine);
#if IBUS_CHECK_VERSION (1, 5, 4)
static void     ibus_pinyin_engine_set_content_type
                                                (IBusEngine     *engine,
                                                 guint           purpose,
                                                 guint           hints);
#endif
static void     ibus_pinyin_engine_reset        (IBusEngine             *engine);
static void     ibus_pinyin_engine_enable       (IBusEngine             *engine);
static void     ibus_pinyin_engine_disable      (IBusEngine             *engine);

#if 0
static void     ibus_engine_set_cursor_location (IBusEngine             *engine,
                                                 gint                    x,
                                                 gint                    y,
                                                 gint                    w,
                                                 gint                    h);
static void     ibus_pinyin_engine_set_capabilities
                                                (IBusEngine             *engine,
                                                 guint                   caps);
#endif

static void     ibus_pinyin_engine_page_up      (IBusEngine             *engine);
static void     ibus_pinyin_engine_page_down    (IBusEngine             *engine);
static void     ibus_pinyin_engine_cursor_up    (IBusEngine             *engine);
static void     ibus_pinyin_engine_cursor_down  (IBusEngine             *engine);
static void     ibus_pinyin_engine_property_activate
                                                (IBusEngine             *engine,
                                                 const gchar            *prop_name,
                                                 guint                   prop_state);
static void     ibus_pinyin_engine_candidate_clicked
                                                (IBusEngine             *engine,
                                                 guint                   index,
                                                 guint                   button,
                                                 guint                   state);
#if 0
static void ibus_pinyin_engine_property_show    (IBusEngine             *engine,
                                                 const gchar            *prop_name);
static void ibus_pinyin_engine_property_hide    (IBusEngine             *engine,
                                                 const gchar            *prop_name);
#endif

G_DEFINE_TYPE (IBusPinyinEngine, ibus_pinyin_engine, IBUS_TYPE_ENGINE)

static void
ibus_pinyin_engine_class_init (IBusPinyinEngineClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);
    IBusEngineClass *engine_class = IBUS_ENGINE_CLASS (klass);

    object_class->constructor = ibus_pinyin_engine_constructor;
    ibus_object_class->destroy = (IBusObjectDestroyFunc) ibus_pinyin_engine_destroy;

    engine_class->process_key_event = ibus_pinyin_engine_process_key_event;

    engine_class->reset = ibus_pinyin_engine_reset;
    engine_class->enable = ibus_pinyin_engine_enable;
    engine_class->disable = ibus_pinyin_engine_disable;

    engine_class->focus_in = ibus_pinyin_engine_focus_in;
    engine_class->focus_out = ibus_pinyin_engine_focus_out;

#if IBUS_CHECK_VERSION (1, 5, 4)
    engine_class->set_content_type = ibus_pinyin_engine_set_content_type;
#endif

    engine_class->page_up = ibus_pinyin_engine_page_up;
    engine_class->page_down = ibus_pinyin_engine_page_down;

    engine_class->cursor_up = ibus_pinyin_engine_cursor_up;
    engine_class->cursor_down = ibus_pinyin_engine_cursor_down;

    engine_class->property_activate = ibus_pinyin_engine_property_activate;

    engine_class->candidate_clicked = ibus_pinyin_engine_candidate_clicked;
}

static void
ibus_pinyin_engine_init (IBusPinyinEngine *pinyin)
{
    if (g_object_is_floating (pinyin))
        g_object_ref_sink (pinyin);  // make engine sink
}

static GObject*
ibus_pinyin_engine_constructor (GType                  type,
                                guint                  n_construct_params,
                                GObjectConstructParam *construct_params)
{
    IBusPinyinEngine *engine;
    const gchar *name;

    engine = (IBusPinyinEngine *) G_OBJECT_CLASS (ibus_pinyin_engine_parent_class)->constructor (
                                                           type,
                                                           n_construct_params,
                                                           construct_params);
    name = ibus_engine_get_name ((IBusEngine *) engine);

    if (name) {
        if (std::strcmp (name, "libpinyin") == 0 ||
            std::strcmp (name, "libpinyin-debug") == 0) {
            engine->engine = new PinyinEngine (IBUS_ENGINE (engine));
        }
        if (std::strcmp (name, "libbopomofo") == 0 ||
            std::strcmp (name, "libbopomofo-debug") == 0 ) {
            engine->engine = new BopomofoEngine (IBUS_ENGINE (engine));
        }
    } else {
        engine->engine = new PinyinEngine (IBUS_ENGINE (engine));
    }
    return (GObject *) engine;
}

static void
ibus_pinyin_engine_destroy (IBusPinyinEngine *pinyin)
{
    delete pinyin->engine;
    ((IBusObjectClass *) ibus_pinyin_engine_parent_class)->destroy ((IBusObject *)pinyin);
}

static gboolean
ibus_pinyin_engine_process_key_event (IBusEngine     *engine,
                                      guint           keyval,
                                      guint           keycode,
                                      guint           modifiers)
{
    IBusPinyinEngine *pinyin = (IBusPinyinEngine *) engine;
    return pinyin->engine->processKeyEvent (keyval, keycode, modifiers);
}

#if IBUS_CHECK_VERSION (1, 5, 4)
static void
ibus_pinyin_engine_set_content_type (IBusEngine     *engine,
                                     guint purpose,
                                     guint hints)
{
    IBusPinyinEngine *pinyin = (IBusPinyinEngine *) engine;
    return pinyin->engine->setContentType (purpose, hints);
}
#endif

static void
ibus_pinyin_engine_property_activate (IBusEngine    *engine,
                                      const gchar   *prop_name,
                                      guint          prop_state)
{
    IBusPinyinEngine *pinyin = (IBusPinyinEngine *) engine;
    pinyin->engine->propertyActivate (prop_name, prop_state);
}
static void
ibus_pinyin_engine_candidate_clicked (IBusEngine *engine,
                                      guint       index,
                                      guint       button,
                                      guint       state)
{
    IBusPinyinEngine *pinyin = (IBusPinyinEngine *) engine;
    pinyin->engine->candidateClicked (index, button, state);
}

#define FUNCTION(name, Name)                                        \
    static void                                                     \
    ibus_pinyin_engine_##name (IBusEngine *engine)                  \
    {                                                               \
        IBusPinyinEngine *pinyin = (IBusPinyinEngine *) engine;     \
        pinyin->engine->Name ();                                    \
        ((IBusEngineClass *) ibus_pinyin_engine_parent_class)       \
            ->name (engine);                                        \
    }
FUNCTION(focus_in,    focusIn)
FUNCTION(focus_out,   focusOut)
FUNCTION(reset,       reset)
FUNCTION(enable,      enable)
FUNCTION(disable,     disable)
FUNCTION(page_up,     pageUp)
FUNCTION(page_down,   pageDown)
FUNCTION(cursor_up,   cursorUp)
FUNCTION(cursor_down, cursorDown)
#undef FUNCTION

Engine::Engine (IBusEngine *engine) : m_engine (engine)
{
#if IBUS_CHECK_VERSION (1, 5, 4)
    m_input_purpose = IBUS_INPUT_PURPOSE_FREE_FORM;
#endif
}

gboolean
Engine::contentIsPassword()
{
#if IBUS_CHECK_VERSION (1, 5, 4)
   return IBUS_INPUT_PURPOSE_PASSWORD == m_input_purpose;
#else
   return FALSE;
#endif
}

void
Engine::focusOut (void)
{
#if IBUS_CHECK_VERSION (1, 5, 4)
    m_input_purpose = IBUS_INPUT_PURPOSE_FREE_FORM;
#endif
}

#if IBUS_CHECK_VERSION(1, 5, 4)
void
Engine::setContentType (guint purpose, guint hints)
{
    m_input_purpose = (IBusInputPurpose) purpose;
}
#endif

Engine::~Engine (void)
{
}

gboolean
pinyin_accelerator_name(guint keyval, guint modifiers, std::string & name) {
    name = "";

    /* Convert some key press to modifiers. */
    switch (keyval) {
    case IBUS_KEY_Control_L:
    case IBUS_KEY_Control_R:
        modifiers |= IBUS_CONTROL_MASK;
        keyval = 0;
        break;
    case IBUS_KEY_Alt_L:
    case IBUS_KEY_Alt_R:
        modifiers |= IBUS_MOD1_MASK;
        keyval = 0;
        break;
    case IBUS_KEY_Shift_L:
    case IBUS_KEY_Shift_R:
        modifiers |= IBUS_SHIFT_MASK;
        keyval = 0;
        break;
    case IBUS_KEY_Meta_L:
    case IBUS_KEY_Meta_R:
        modifiers |= IBUS_META_MASK;
        keyval = 0;
        break;
    case IBUS_KEY_Super_L:
    case IBUS_KEY_Super_R:
        modifiers |= IBUS_SUPER_MASK;
        keyval = 0;
        break;
    case IBUS_KEY_Hyper_L:
    case IBUS_KEY_Hyper_R:
        modifiers |= IBUS_HYPER_MASK;
        keyval = 0;
        break;
    }

    /* Convert modifiers. */
    if (modifiers & IBUS_CONTROL_MASK)
        name += "<Control>";
    if (modifiers & IBUS_MOD1_MASK)
        name += "<Alt>";
    if (modifiers & IBUS_SHIFT_MASK)
        name += "<Shift>";
    if (modifiers & IBUS_META_MASK)
        name += "<Meta>";
    if (modifiers & IBUS_SUPER_MASK)
        name += "<Super>";
    if (modifiers & IBUS_HYPER_MASK)
        name += "<Hyper>";


    /* Convert keyval. */
    if (keyval) {
        const gchar * symbol = ibus_keyval_name (ibus_keyval_to_lower (keyval));
        name += symbol;
    }

    return TRUE;
}

};

