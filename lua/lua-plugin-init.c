/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2010 Peng Wu <alexepico@gmail.com>
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


#include <stdio.h>
#include <string.h>
#include <glib.h>

#include "lua-plugin.h"

#if LUA_VERSION_NUM >= 502
/* ugly hack for lua 5.2 */

#ifndef lua_objlen
#define lua_objlen lua_rawlen
#endif

#endif


static const luaL_Reg lualibs[] = {
  {"", luaopen_base},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_IOLIBNAME, luaopen_io},
  {LUA_OSLIBNAME, luaopen_myos},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_IMELIBNAME, luaopen_ime},
  {NULL, NULL}
};


void lua_plugin_openlibs (lua_State *L) {
  const luaL_Reg *lib = lualibs;
  for (; lib->func; lib++) {
#if LUA_VERSION_NUM >= 502
    luaL_requiref(L, lib->name, lib->func, TRUE);
#else
    lua_pushcfunction(L, lib->func);
    lua_pushstring(L, lib->name);
    lua_call(L, 1, 0);
#endif
  }
}

void lua_plugin_store_plugin(lua_State * L, IBusEnginePlugin * plugin){
  luaL_newmetatable(L, LUA_IMELIBNAME);
  lua_pushliteral(L, LUA_IMELIB_CONTEXT);
  lua_pushlightuserdata(L, plugin);
  lua_rawset(L, -3);
  lua_pop(L, 1);
}

IBusEnginePlugin * lua_plugin_retrieve_plugin(lua_State * L) {
  luaL_newmetatable(L, LUA_IMELIBNAME);
  lua_pushliteral(L, LUA_IMELIB_CONTEXT);
  lua_rawget(L, -2);
  luaL_checktype(L, -1, LUA_TLIGHTUSERDATA);
  IBusEnginePlugin * plugin = lua_touserdata(L, -1);
  g_assert(IBUS_IS_ENGINE_PLUGIN(plugin));
  lua_pop(L, 2);
  return plugin;
}

static int ime_get_last_commit(lua_State* L){
  /*TODO: not implemented. */
  fprintf(stderr, "TODO: ime_get_last_commit unimplemented.\n");
  lua_pushstring(L, "");
  return 1;
}

static int ime_get_version(lua_State* L){
  /* TODO: replace this with C macros. */
  lua_pushliteral(L, "ibus-libpinyin 1.4.0");
  return 1;
}

static int ime_int_to_hex_string(lua_State* L){
    lua_Integer val = luaL_checkinteger(L, 1);
    lua_Integer width = luaL_optinteger(L, 2, -1);

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    gchar * str = g_strdup_printf("%0*x", width, val);
    luaL_addstring(&buf, str);
    g_free(str);

    luaL_pushresult(&buf);
    lua_remove(L, 2);
    lua_remove(L, 1);
    return 1;
}

static int ime_join_string(lua_State* L){
  luaL_Buffer buf;
  size_t vec_len; size_t i;
  const char * sep;
  const char * str;

  luaL_checktype(L, 1, LUA_TTABLE);

  sep = luaL_checklstring(L, 2, NULL);
  vec_len = lua_objlen(L, 1);

  if ( 0 == vec_len ){
    lua_pop(L, 2);
    lua_pushliteral(L, "");
    return 1;
  }

  luaL_buffinit(L, &buf);

  for ( i = 1; i < vec_len; ++i){
    lua_pushinteger(L, i);
    lua_gettable(L, 1);
    str = luaL_checklstring(L, 3, NULL);
    luaL_addstring(&buf, str);
    lua_pop(L, 1);
    luaL_addstring(&buf, sep);
  }

  /* add tail of string list */
  lua_pushinteger(L, i);
  lua_gettable(L, 1);
  str = luaL_checklstring(L, 3, NULL);
  luaL_addstring(&buf, str);
  lua_pop(L, 1);
  /* remove the args. */
  lua_pop(L, 2);
  luaL_pushresult(&buf);

  return 1;
}

static int ime_parse_mapping(lua_State * L){
  const char * src_string, * line_sep, * key_value_sep, * values_sep;
  int m, n;
  gchar** lines = NULL; size_t lines_no = 0; const char * line;
  gchar** key_value = NULL; const char * key = NULL;
  gchar** values = NULL; size_t values_no = 0; const char * value = NULL;

  src_string = luaL_checklstring(L, 1, NULL);
  line_sep = luaL_checklstring(L, 2, NULL);
  key_value_sep = luaL_checklstring(L, 3, NULL);
  values_sep = luaL_checklstring(L, 4, NULL);
  
  lines = g_strsplit(src_string, line_sep, 0);
  lines_no = g_strv_length(lines);
  lua_createtable(L, 0, lines_no);
  for( m = 0; m < lines_no; ++m){
    line = lines[m];
    if ( NULL == line || '\0' == line[0])
      continue;
    key_value = g_strsplit(line, key_value_sep, 2);
    key = key_value[0]; /* value = key_value[1]; */
    if ( NULL == key || '\0' == key[0])
      continue;
    {
      values = g_strsplit(key_value[1], values_sep, 0);
      values_no = g_strv_length(values);
      lua_createtable(L, values_no, 0);
      for ( n = 0; n < values_no; ++n){
        value = values[n];
        if ( NULL == value || '\0' == value[0] )
          continue;
        lua_pushinteger(L, n + 1);
        lua_pushstring(L, value);
        lua_settable(L, 6);
      }
      g_strfreev(values);
    }

    lua_pushstring(L, key);
    lua_insert(L, 6);
    lua_settable(L, 5);
    g_strfreev(key_value);
  }

  g_strfreev(lines);
  /*remove args */
  lua_remove(L, 4);
  lua_remove(L, 3);
  lua_remove(L, 2);
  lua_remove(L, 1);
  return 1;
}

static int ime_register_command(lua_State * L){
  lua_command_t new_command;
  size_t l;

  memset(&new_command, 0, sizeof(new_command));
  new_command.command_name = luaL_checklstring(L, 1, &l);
  if ( 2 != l ){
    return luaL_error(L, "ime_register_command is called with command_name: %s, whose length is not 2.\n", new_command.command_name);
  }

  new_command.lua_function_name = luaL_checklstring(L, 2, NULL);
  lua_getglobal(L, new_command.lua_function_name);
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_pop(L, 1);

  new_command.description = luaL_checklstring(L, 3, NULL);

  if ( !lua_isnone(L, 4)) {
    new_command.leading = luaL_checklstring(L, 4, NULL);
  }else{
    new_command.leading = "digit";
  }

  if ( !lua_isnone(L, 5)) {
    new_command.help = luaL_checklstring(L, 5, NULL);
  }

  gboolean result = ibus_engine_plugin_add_command
    (lua_plugin_retrieve_plugin(L), &new_command);

  if (!result)
    return luaL_error(L, "register command %s with function %s failed.\n", new_command.command_name, new_command.lua_function_name);

  return 0;
}

static int ime_register_trigger(lua_State * L){
  lua_trigger_t new_trigger;

  memset(&new_trigger, 0, sizeof(new_trigger));
  new_trigger.lua_function_name = luaL_checklstring(L, 1, NULL);
  lua_getglobal(L, new_trigger.lua_function_name);
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_pop(L, 1);

  new_trigger.description = luaL_checklstring(L, 2, NULL);
  size_t num; gint i;
  GPtrArray *array;

  /* register_trigger with input_trigger_strings. */
  array = g_ptr_array_new();
  luaL_checktype(L, 3, LUA_TTABLE);
  num = lua_objlen(L, 3);
  for ( i = 0; i < num; ++i) {
    lua_pushinteger(L, i + 1);
    lua_gettable(L, 3);
    g_ptr_array_add(array, (gpointer)lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  g_ptr_array_add(array, NULL);
  new_trigger.input_trigger_strings =
    (gchar **)g_ptr_array_free(array, FALSE);

  /* register_trigger with candidate_trigger_strings. */
  array = g_ptr_array_new();
  luaL_checktype(L, 4, LUA_TTABLE);
  num = lua_objlen(L, 4);
  for ( i = 0; i < num; ++i) {
    lua_pushinteger(L, i + 1);
    lua_gettable(L, 4);
    g_ptr_array_add(array, (gpointer)lua_tostring(L, -1));
    lua_pop(L, 1);
  }
  g_ptr_array_add(array, NULL);
  new_trigger.candidate_trigger_strings =
    (gchar **)g_ptr_array_free(array, FALSE);

  gboolean result = ibus_engine_plugin_add_trigger
    (lua_plugin_retrieve_plugin(L), &new_trigger);

  g_free(new_trigger.input_trigger_strings);
  g_free(new_trigger.candidate_trigger_strings);

  if (!result)
    return luaL_error(L, "register trigger with function %s failed.\n", new_trigger.lua_function_name);

  return 0;
}

static int ime_register_converter(lua_State * L){
  lua_converter_t new_converter;

  memset(&new_converter, 0, sizeof(new_converter));
  new_converter.lua_function_name = luaL_checklstring(L, 1, NULL);
  lua_getglobal(L, new_converter.lua_function_name);
  luaL_checktype(L, -1, LUA_TFUNCTION);
  lua_pop(L, 1);

  new_converter.description = luaL_checklstring(L, 2, NULL);

  gboolean result = ibus_engine_plugin_add_converter
    (lua_plugin_retrieve_plugin(L), &new_converter);

  if (!result)
    return luaL_error(L, "register converter with function %s failed.\n", new_converter.lua_function_name);

  return 0;
}

static int ime_split_string(lua_State * L){
  gchar ** str_vec;
  guint str_vec_len = 0; int i;
  const char * sep;
  const char * str = luaL_checklstring(L, 1, NULL);

  sep = luaL_checklstring(L, 2, NULL);

  str_vec = g_strsplit(str, sep, 0);
  str_vec_len = g_strv_length(str_vec);

  lua_createtable(L, str_vec_len, 0);
  for ( i = 0; i < str_vec_len; ++i){
    lua_pushinteger(L, i + 1);
    lua_pushstring(L, str_vec[i]);
    lua_settable(L, 3);
  }

  g_strfreev(str_vec);
  lua_remove(L, 2); /* remove sep from stack */
  lua_remove(L, 1); /* remove str from stack */
  return 1;
}

static gboolean ime_is_white_space(const char c){
  static const char * const white_space = " \t\n\r\v\f";
  int i;
  size_t len = strlen(white_space);
  
  for ( i = 0; i < len; ++i){
    if ( white_space[i] == c )
      return TRUE;
  }
  return FALSE;
}

static int ime_push_string(lua_State* L, const char * s, 
                                int start, int end){
  if (start >= end ){
    lua_pushliteral(L, "");
    return 1;
  }
  lua_pushlstring(L, s + start, end -start);
  lua_remove(L, 1);
  return 1;
}

static int ime_trim_string_left(lua_State* L){
  size_t l; int start, end;
  const char * s = luaL_checklstring(L, 1, &l);

  start = 0; end = l;
  while( ime_is_white_space(s[start])){
    start++;
  }

  return ime_push_string(L, s, start, end);
}

static int ime_trim_string_right(lua_State* L){
  size_t l; int start, end;
  const char * s = luaL_checklstring(L, 1, &l);

  start = 0; end = l;
  while( ime_is_white_space(s[end - 1]) && end > 0){
    end--;
  }

  return ime_push_string(L, s, start, end);
}

static int ime_trim_string(lua_State* L){
  size_t l; int start, end;
  const char * s = luaL_checklstring(L, 1, &l);

  start = 0; end = l;
  while( ime_is_white_space(s[start])){
    start++;
  }
  while( ime_is_white_space(s[end - 1]) && end > 0){
    end--;
  }

  return ime_push_string(L, s, start, end);
}

static int ime_utf8_to_utf16(lua_State* L){
    size_t l;
    const char * s = luaL_checklstring(L, 1, &l);

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    glong written = 0;
    gunichar2 * str = g_utf8_to_utf16(s, l, NULL, &written, NULL);

    /* not includes trailing-zero */
    luaL_addlstring(&buf, (const char *)str, written * sizeof(gunichar2));
    luaL_pushresult(&buf);

    g_free(str);
    lua_remove(L, 1);
    return 1;
}

static int ime_utf16_to_utf8(lua_State* L){
    size_t l;
    const gunichar2 * s = (const gunichar2 *)luaL_checklstring(L, 1, &l);

    luaL_Buffer buf;
    luaL_buffinit(L, &buf);

    glong written = 0;
    gchar * str = g_utf16_to_utf8(s, l / sizeof(gunichar2),
                                  NULL, &written, NULL );

    /* not includes trailing-zero */
    luaL_addlstring(&buf, str, written * sizeof(gchar));
    luaL_pushresult(&buf);

    g_free(str);
    lua_remove(L, 1);
    return 1;
}

static const luaL_Reg imelib[] = {
  {"get_last_commit", ime_get_last_commit},
  {"get_version", ime_get_version},
  {"int_to_hex_string", ime_int_to_hex_string},
  {"join_string", ime_join_string},
  {"parse_mapping", ime_parse_mapping},
  {"register_command", ime_register_command},
  {"register_converter", ime_register_converter},
  {"register_trigger", ime_register_trigger},
  {"split_string", ime_split_string},
  {"trim_string_left", ime_trim_string_left},
  {"trim_string_right", ime_trim_string_right},
  {"trim_string", ime_trim_string},
  {"utf16_to_utf8", ime_utf16_to_utf8},
  {"utf8_to_utf16", ime_utf8_to_utf16},
  {NULL, NULL}
};

LUALIB_API int luaopen_ime (lua_State *L) {
#if LUA_VERSION_NUM >= 502
  luaL_newlib(L, imelib);
#else
  luaL_register(L, LUA_IMELIBNAME, imelib);
#endif
  return 1;
}

