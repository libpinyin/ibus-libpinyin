/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2010 Peng Wu <alexepico@gmail.com>
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


#include <string.h>
#include <stdlib.h>

#include "lua-plugin.h"

#if LUA_VERSION_NUM >= 502
/* ugly hack for lua 5.2 */

#define lua_open luaL_newstate

#ifndef lua_objlen
#define lua_objlen lua_rawlen
#endif

#endif

struct _IBusEnginePluginPrivate{
  lua_State * L;
  GArray * lua_commands; /* Array of lua_command_t. */
  GArray * lua_triggers; /* Array of lua_trigger_t. */
  GArray * lua_converters; /* Array of lua_converter_t. */
  gchar * use_converter;
};

G_DEFINE_TYPE_WITH_CODE (IBusEnginePlugin, ibus_engine_plugin, G_TYPE_OBJECT, G_ADD_PRIVATE (IBusEnginePlugin));

#define IBUS_ENGINE_PLUGIN_GET_PRIVATE(obj) (ibus_engine_plugin_get_instance_private (obj))

static void lua_command_clone(lua_command_t * command, lua_command_t * new_command){
  new_command->command_name = g_strdup(command->command_name);
  new_command->lua_function_name = g_strdup(command->lua_function_name);
  new_command->description = g_strdup(command->description);
  new_command->leading = g_strdup(command->leading);
  new_command->help = g_strdup(command->help);
}

static void lua_command_reclaim(lua_command_t * command){
  g_free((gpointer)command->command_name);
  g_free((gpointer)command->lua_function_name);
  g_free((gpointer)command->description);
  g_free((gpointer)command->leading);
  g_free((gpointer)command->help);
}

static void lua_trigger_clone(lua_trigger_t * trigger, lua_trigger_t * new_trigger){
  new_trigger->lua_function_name = g_strdup(trigger->lua_function_name);
  new_trigger->description = g_strdup(trigger->description);
  new_trigger->input_trigger_strings = g_strdupv(trigger->input_trigger_strings);
  new_trigger->candidate_trigger_strings = g_strdupv(trigger->candidate_trigger_strings);
}

static void lua_trigger_reclaim(lua_trigger_t * trigger){
  g_free((gpointer)trigger->lua_function_name);
  g_free((gpointer)trigger->description);
  g_strfreev((gchar **)trigger->input_trigger_strings);
  g_strfreev((gchar **)trigger->candidate_trigger_strings);
}

static void lua_converter_clone(lua_converter_t * converter, lua_converter_t * new_converter){
  new_converter->lua_function_name = g_strdup(converter->lua_function_name);
  new_converter->description = g_strdup(converter->description);
}

static void lua_converter_reclaim(lua_converter_t * converter){
  g_free((gpointer)converter->lua_function_name);
  g_free((gpointer)converter->description);
}

static int
lua_plugin_init(IBusEnginePluginPrivate * plugin){
  g_assert(NULL == plugin->L);
  /* initialize Lua */
  plugin->L = lua_open();

  /* enable libs in sandbox */
  lua_plugin_openlibs(plugin->L);

  g_assert ( NULL == plugin->lua_commands );
  plugin->lua_commands = g_array_new(TRUE, TRUE, sizeof(lua_command_t));

  g_assert ( NULL == plugin->lua_triggers );
  plugin->lua_triggers = g_array_new(TRUE, TRUE, sizeof(lua_trigger_t));

  g_assert ( NULL == plugin->lua_converters );
  plugin->lua_converters = g_array_new(TRUE, TRUE, sizeof(lua_converter_t));
  plugin->use_converter = NULL;

  return 0;
}

static int
lua_plugin_fini(IBusEnginePluginPrivate * plugin){
  size_t i;
  lua_command_t * command;
  lua_trigger_t * trigger;
  lua_converter_t * converter;

  if ( plugin->lua_commands ){
    for ( i = 0; i < plugin->lua_commands->len; ++i){
      command = &g_array_index(plugin->lua_commands, lua_command_t, i);
      lua_command_reclaim(command);
    }
    g_array_free(plugin->lua_commands, TRUE);
    plugin->lua_commands = NULL;
  }

  if ( plugin->lua_triggers ){
    for ( i = 0; i < plugin->lua_triggers->len; ++i){
      trigger = &g_array_index(plugin->lua_triggers, lua_trigger_t, i);
      lua_trigger_reclaim(trigger);
    }
    g_array_free(plugin->lua_triggers, TRUE);
    plugin->lua_triggers = NULL;
  }

  if ( plugin->lua_converters ){
    for ( i = 0; i < plugin->lua_converters->len; ++i){
      converter = &g_array_index(plugin->lua_converters, lua_converter_t, i);
      lua_converter_reclaim(converter);
    }
    g_array_free(plugin->lua_converters, TRUE);
    plugin->lua_converters = NULL;
  }

  lua_close(plugin->L);
  plugin->L = NULL;

  g_free(plugin->use_converter);
  plugin->use_converter = NULL;

  return 0;
}

static void
ibus_engine_plugin_finalize (GObject *gobject)
{
  IBusEnginePlugin *self = IBUS_ENGINE_PLUGIN (gobject);

  /* do some cleaning here. */
  lua_plugin_fini(self->priv);

  /* Chain up to the parent class */
  G_OBJECT_CLASS (ibus_engine_plugin_parent_class)->dispose(gobject);
}

static void
ibus_engine_plugin_class_init (IBusEnginePluginClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = ibus_engine_plugin_finalize;
}

static void
ibus_engine_plugin_init (IBusEnginePlugin *self)
{
  IBusEnginePluginPrivate *priv;

  self->priv = priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE (self);

  memset(priv, 0, sizeof(IBusEnginePluginPrivate));

  lua_plugin_init(priv);

  lua_plugin_store_plugin(priv->L, self);
}

IBusEnginePlugin * ibus_engine_plugin_new(){
  IBusEnginePlugin * plugin;

  plugin = (IBusEnginePlugin *) g_object_new (IBUS_TYPE_ENGINE_PLUGIN,
                                              NULL);

  return plugin;
}

static void l_message (const char *pname, const char *msg) {
  if (pname) fprintf(stderr, "%s: ", pname);
  fprintf(stderr, "%s\n", msg);
  fflush(stderr);
}

static int report (lua_State *L, int status) {
  if (status && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(NULL, msg);
    lua_pop(L, 1);
  }
  return status;
}

int ibus_engine_plugin_load_lua_script(IBusEnginePlugin * plugin, const char * filename){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  int status = luaL_dofile(priv->L, filename);
  return report(priv->L, status);
}


static gint compare_command(gconstpointer a, gconstpointer b){
  lua_command_t * ca = (lua_command_t *) a;
  lua_command_t * cb = (lua_command_t *) b;
  return strcmp(ca->command_name, cb->command_name);
}

gboolean ibus_engine_plugin_add_command(IBusEnginePlugin * plugin, lua_command_t * command){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_commands = priv->lua_commands;

  if ( ibus_engine_plugin_lookup_command( plugin, command->command_name) )
    return FALSE;

  lua_command_t new_command;
  lua_command_clone(command, &new_command);

  g_array_append_val(lua_commands, new_command);
  /* Note: need to improve speed here? */
  g_array_sort(lua_commands, compare_command);

  return TRUE;
}

const lua_command_t * ibus_engine_plugin_lookup_command(IBusEnginePlugin * plugin, const char * command_name){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_commands = priv->lua_commands;
  lua_command_t lookup_command = {.command_name = command_name, };

  lua_command_t * result = bsearch(&lookup_command, lua_commands->data, lua_commands->len, sizeof(lua_command_t), compare_command);
  return result;
}

const GArray * ibus_engine_plugin_get_available_commands(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  return priv->lua_commands;
}

gboolean ibus_engine_plugin_add_trigger(IBusEnginePlugin * plugin, lua_trigger_t * trigger){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_triggers = priv->lua_triggers;

  lua_trigger_t new_trigger;
  lua_trigger_clone(trigger, &new_trigger);

  g_array_append_val(lua_triggers, new_trigger);

  return TRUE;
}

const GArray * ibus_engine_plugin_get_available_triggers(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  return priv->lua_triggers;
}

gboolean ibus_engine_plugin_match_input(IBusEnginePlugin * plugin, const char * input, const char ** lua_function_name){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_triggers = priv->lua_triggers;

  gint i; gchar ** pattern;
  for (i = 0; i < lua_triggers->len; ++i){
    lua_trigger_t * trigger = &g_array_index(lua_triggers, lua_trigger_t, i);

    for (pattern = trigger->input_trigger_strings; *pattern != NULL; ++pattern){
      if (g_pattern_match_simple(*pattern, input)){
        *lua_function_name = trigger->lua_function_name;
        return TRUE;
      }
    }
  }

  return FALSE;
}

gboolean ibus_engine_plugin_match_candidate(IBusEnginePlugin * plugin, const char * candidate, const char ** lua_function_name){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_triggers = priv->lua_triggers;

  gint i; gchar ** pattern;
  for (i = 0; i < lua_triggers->len; ++i){
    lua_trigger_t * trigger = &g_array_index(lua_triggers, lua_trigger_t, i);

    for (pattern = trigger->candidate_trigger_strings; *pattern != NULL; ++pattern){
      if (g_pattern_match_simple(*pattern, candidate)){
        *lua_function_name = trigger->lua_function_name;
        return TRUE;
      }
    }
  }

  return FALSE;
}

gboolean ibus_engine_plugin_add_converter(IBusEnginePlugin * plugin, lua_converter_t * converter){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_converters = priv->lua_converters;

  lua_converter_t new_converter;
  lua_converter_clone(converter, &new_converter);

  g_array_append_val(lua_converters, new_converter);

  return TRUE;
}

const GArray * ibus_engine_plugin_get_available_converters(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  return priv->lua_converters;
}

gboolean ibus_engine_plugin_set_converter(IBusEnginePlugin * plugin, const char * lua_function_name){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  GArray * lua_converters = priv->lua_converters;

  gint i;
  for (i = 0; i < lua_converters->len; ++i) {
    lua_converter_t * converter = &g_array_index
      (lua_converters, lua_converter_t, i);
    if (g_strcmp0 (converter->lua_function_name, lua_function_name) == 0) {
      g_free(priv->use_converter);
      priv->use_converter = g_strdup(lua_function_name);
      return TRUE;
    }
  }

  return FALSE;
}

const char * ibus_engine_plugin_get_converter(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  return priv->use_converter;  
}

int ibus_engine_plugin_call(IBusEnginePlugin * plugin, const char * lua_function_name, const char * argument /*optional, maybe NULL.*/){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  int type; int result;

  lua_State * L = priv->L;

  /* In google pinyin, argument can't be NULL,
     but empty string is acceptable. */
  if (NULL == argument) argument = "";

  /* check whether lua_function_name exists. */
  lua_getglobal(L, lua_function_name);
  type = lua_type(L, -1);
  if ( LUA_TFUNCTION != type )
    return 0;
  lua_pushstring(L, argument);

  result = lua_pcall(L, 1, 1, 0);
  if (result) return 0;

  type = lua_type(L, -1);
  if ( LUA_TTABLE == type ){
    return lua_objlen(L, -1);
  } else if (LUA_TNUMBER == type || LUA_TBOOLEAN == type || LUA_TSTRING == type){
    return 1;
  }

  return 0;
}

/**
 * get a candidate from lua return value.
 */
static const lua_command_candidate_t * ibus_engine_plugin_get_candidate(lua_State * L){

  const char * suggest, * help, * content = NULL;
  lua_command_candidate_t * candidate = malloc(sizeof(lua_command_candidate_t));

  memset(candidate, 0, sizeof(lua_command_candidate_t));

  int type = lua_type(L, -1);

  if ( LUA_TTABLE == type ){
    lua_pushliteral(L, "suggest");
    lua_gettable(L, -2);
    lua_pushliteral(L, "help");
    lua_gettable(L, -3);
    suggest = lua_tostring(L, -2);
    help = lua_tostring(L, -1);
    candidate->suggest = g_strdup(suggest);
    candidate->help = g_strdup(help);
    lua_pop(L, 2);
  } else if (LUA_TNUMBER == type || LUA_TBOOLEAN == type || LUA_TSTRING == type) {
    content = lua_tostring(L, -1);
    candidate->content = g_strdup(content);
  }

  return candidate;
}

/**
 * retrieve the first string value. (value has been copied.)
 */
gchar * ibus_engine_plugin_get_first_result(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  const char * result = NULL; int type;
  lua_State * L = priv->L;

  type = lua_type(L ,-1);
  if ( LUA_TNUMBER == type || LUA_TBOOLEAN == type || LUA_TSTRING == type) {
    result = g_strdup(lua_tostring(L, -1));
    lua_pop(L, 1);
  } else if( LUA_TTABLE == type ){
    lua_pushinteger(L, 1);
    lua_gettable(L, -2);
    int type = lua_type(L, -1);
    if ( LUA_TNUMBER == type || LUA_TBOOLEAN == type || LUA_TSTRING == type )
      result = g_strdup(lua_tostring(L, -1));
    lua_pop(L, 2);
  }

  return (char *)result;
}

/**
 * retrieve the retval string value. (value has been copied.)
 */
const lua_command_candidate_t * ibus_engine_plugin_get_retval(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  lua_command_candidate_t * result = NULL; int type;
  lua_State * L = priv->L;

  type = lua_type(L ,-1);
  if ( LUA_TNUMBER == type || LUA_TBOOLEAN == type || LUA_TSTRING == type) {
    result = malloc(sizeof(lua_command_candidate_t));
    memset(result, 0, sizeof(lua_command_candidate_t));
    result->content = g_strdup(lua_tostring(L, -1));
    lua_pop(L, 1);
  } else if( LUA_TTABLE == type ){
    lua_pushinteger(L, 1);
    lua_gettable(L, -2);
    result = (lua_command_candidate_t *)ibus_engine_plugin_get_candidate(L);
    lua_pop(L, 2);
  }

  return (const lua_command_candidate_t *)result;
}

/**
 * retrieve the array of string values. (string values have been copied.)
 */
GArray * ibus_engine_plugin_get_retvals(IBusEnginePlugin * plugin){
  IBusEnginePluginPrivate * priv = IBUS_ENGINE_PLUGIN_GET_PRIVATE(plugin);
  lua_State * L = priv->L; int elem_num; int type;
  GArray * result =  NULL; int i;
  const lua_command_candidate_t * candidate = NULL;

  type = lua_type(L, -1);
  if ( LUA_TTABLE != type )
    return result;

  result = g_array_new(TRUE, TRUE, sizeof(lua_command_candidate_t *));
  elem_num = lua_objlen(L, -1);

  for ( i = 0; i < elem_num; ++i ){
    lua_pushinteger(L, i + 1);
    lua_gettable(L, -2);

    candidate = ibus_engine_plugin_get_candidate(L);
    lua_pop(L, 1);

    g_array_append_val(result, candidate);
  }

  lua_pop(L, 1);
  return result;
}

void ibus_engine_plugin_free_candidate(lua_command_candidate_t * candidate){
  g_free((gpointer)candidate->content);
  g_free((gpointer)candidate->suggest);
  g_free((gpointer)candidate->help);
}
