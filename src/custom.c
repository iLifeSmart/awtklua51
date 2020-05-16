#include "base/enums.h"
#include "tkc/emitter.h"
#include "ui_loader/ui_builder_default.h"

typedef struct _userdata_info_t {
  const char* info;
  void* data;
} userdata_info_t;

//static lua_State* s_current_L = NULL;
extern void luaL_openlib(lua_State* L, const char* libname, const luaL_Reg* l, int nup);

static int tk_newuserdata(lua_State* L, void* data, const char* info, const char* metatable) {
  char str[48];
  userdata_info_t* udata = NULL;
  return_value_if_fail(data != NULL, 0);

  udata = (userdata_info_t*)lua_newuserdata(L, sizeof(userdata_info_t));
  return_value_if_fail(data != NULL, 0);

  udata->data = data;
  udata->info = info;

  if (strstr(info, "/widget_t") != NULL && strcmp(metatable, "awtk.widget_t") == 0) {
    widget_t* widget = (widget_t*)data;
    const char* type = widget_get_type(widget);
    snprintf(str, sizeof(str), "awtk.%s_t", type);
    metatable = str;
  }

  if (metatable != NULL) {
    //hack by pulleyzzz
    int ret = (luaL_getmetatable(L, metatable),lua_type((L), -1));
    if (ret == 0 && strstr(info, "/widget_t") != NULL) {
      lua_pop(L, 1);
      ret = (luaL_getmetatable(L, "awtk.widget_t"),lua_type((L), -1));
    }
    lua_setmetatable(L, -2);
  }

  return 1;
}

static const luaL_Reg* find_member(const luaL_Reg* funcs, const char* name) {
  const luaL_Reg* iter = funcs;

  while (iter->name) {
    if (*iter->name == *name && strcmp(iter->name, name) == 0) {
      return iter;
    }
    iter++;
  }

  return NULL;
}

static void* tk_checkudata(lua_State* L, int idx, const char* name) {
  userdata_info_t* udata = (userdata_info_t*)lua_touserdata(L, idx);
  if (udata) {
    // assert(strstr(udata->info, name) != NULL);
    return udata->data;
  } else {
    return NULL;
  }
}

//hack by pulleyzzz
#define AWTK_CALLBACK_ARG_MT "ZAWTK_CALLBACK_ARG_MT"

typedef struct
{
	lua_State* loop_L;
  int l_Ref;
  int callbackRef;
  int udRef;
} awtkluacb_base;

static void freeluacallfun(awtkluacb_base * cbud)
{
  lua_State* L = cbud->loop_L;
  if (L)
  {
    cbud->loop_L=NULL;
    luaL_unref(L, LUA_REGISTRYINDEX, cbud->l_Ref);
    luaL_unref(L, LUA_REGISTRYINDEX, cbud->callbackRef);
	  luaL_unref(L, LUA_REGISTRYINDEX, cbud->udRef);
  }
}
static int awtk_cb_gc(lua_State* L)
{
	awtkluacb_base* arg = (awtkluacb_base*)luaL_checkudata(L, 1, AWTK_CALLBACK_ARG_MT);
	freeluacallfun(arg);
  //printf("gc awtkcb-=-------------\n");
	return 0;
}

int awtk_callback_register(lua_State* L)
{
	luaL_newmetatable(L, AWTK_CALLBACK_ARG_MT);
	lua_pushcfunction(L, awtk_cb_gc);
	lua_setfield(L, -2, "__gc");
	lua_newtable(L);
	lua_pushcfunction(L, awtk_cb_gc);
	lua_setfield(L, -2, "close");
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	return 0;
}

static awtkluacb_base * pushluacallfun(lua_State* L,int callbackIdx )
{
  awtkluacb_base *cbud=(awtkluacb_base *)lua_newuserdata(L, sizeof(awtkluacb_base));
  cbud->loop_L=L;
	luaL_getmetatable(L, AWTK_CALLBACK_ARG_MT);
	lua_setmetatable(L, -2);
	lua_pushvalue(L, -1);
	cbud->udRef = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pushvalue(L, callbackIdx);
	cbud->callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushthread(L);
	cbud->l_Ref = luaL_ref(L, LUA_REGISTRYINDEX);
	return cbud;
}

static void getluacallfun(awtkluacb_base * cbud)
{
  lua_State* L = cbud->loop_L;
  lua_rawgeti(L, LUA_REGISTRYINDEX, cbud->callbackRef);
  //printf("get cb fun=========\n");
}


static ret_t call_on_event(void* ctx, event_t* e) {
  //lua_State* L = (lua_State*)s_current_L;
  //int func = (char*)ctx - (char*)NULL;
  lua_State* L = ((awtkluacb_base *)ctx)->loop_L;
  ret_t ret=RET_OK;
  lua_settop(L, 0);
  //lua_rawgeti(L, LUA_REGISTRYINDEX, func);
  getluacallfun((awtkluacb_base *)ctx);
  tk_newuserdata(L, e, "/event_t", "awtk.event_t");
  if (lua_pcall(L, 1, 1, 0)!=0)
	{
		const char* err=lua_tostring(L,-1);
		printf("%s\n", err);
  }
  else
  {
    ret=luaL_optinteger(L,-1,RET_OK);
  }
  lua_pop(L, 1);
  return RET_OK;
}

static ret_t emitter_item_on_destroy(void* data) {
  emitter_item_t* item = (emitter_item_t*)data;
  lua_State* L = (lua_State*)item->on_destroy_ctx;

  // uint32_t func = (char*)(item->ctx) - (char*)NULL;
  // luaL_unref(L, LUA_REGISTRYINDEX, func);
  freeluacallfun((awtkluacb_base*)(item->ctx));
  return RET_OK;
}

static int wrap_widget_on(lua_State* L) {
  ret_t ret = 0;
  widget_t* widget = (widget_t*)tk_checkudata(L, 1, "widget_t");
  event_type_t type = (event_type_t)luaL_checkinteger(L, 2);
  if (lua_isfunction(L, 3)) {
    // int func = 0;
    // lua_pushvalue(L, 3);
    // func = luaL_ref(L, LUA_REGISTRYINDEX);
    awtkluacb_base *func=pushluacallfun(L,3);
    ret = (ret_t)widget_on(widget, type, call_on_event, func);
    emitter_set_on_destroy(widget->emitter, ret, emitter_item_on_destroy, L);
    lua_pushnumber(L, (lua_Number)ret);
    return 1;
  } else {
    return 0;
  }
}
static int wrap_widget_on_with_tag(lua_State* L) {
  ret_t ret = 0;
  widget_t* widget = (widget_t*)tk_checkudata(L, 1, "widget_t");
  event_type_t type = (event_type_t)luaL_checkinteger(L, 2);
  uint32_t tag=(uint32_t)luaL_optnumber(L,4,0);
  if (lua_isfunction(L, 3)) {
    // int func = 0;
    // lua_pushvalue(L, 3);
    // func = luaL_ref(L, LUA_REGISTRYINDEX);
    awtkluacb_base *func=pushluacallfun(L,3);
    ret = (ret_t)widget_on_with_tag(widget, type, call_on_event, func,tag);
    emitter_set_on_destroy(widget->emitter, ret, emitter_item_on_destroy, L);
    lua_pushnumber(L, (lua_Number)ret);
    return 1;
  } else {
    return 0;
  }
}
static int wrap_emitter_on(lua_State* L) {
  ret_t ret = 0;
  emitter_t* emitter = (emitter_t*)tk_checkudata(L, 1, "emitter_t");
  event_type_t type = (event_type_t)luaL_checkinteger(L, 2);

  if (lua_isfunction(L, 3)) {
    // int func = 0;
    // lua_pushvalue(L, 3);
    // func = luaL_ref(L, LUA_REGISTRYINDEX);
    awtkluacb_base *func=pushluacallfun(L,3);
    ret = (ret_t)emitter_on(emitter, type, call_on_event, func);
    emitter_set_on_destroy(emitter, ret, emitter_item_on_destroy, L);
    lua_pushnumber(L, (lua_Number)ret);
    return 1;
  } else {
    return 0;
  }
}

struct tmpcalloneach_widaaa
{
  lua_State* L;
  int func;
};
static ret_t call_on_each(void* ctx, const void* widget) {
  struct tmpcalloneach_widaaa* tmp=(struct tmpcalloneach_widaaa*)ctx;
  lua_State* L = tmp->L;
  int func = tmp->func;
  ret_t ret=RET_OK;
  lua_settop(L, 0);
  lua_rawgeti(L, LUA_REGISTRYINDEX, func);
  tk_newuserdata(L, WIDGET(widget), "/widget_t", "awtk.widget_t");
  if (lua_pcall(L, 1, 1, 0)!=0)
	{
		const char* err=lua_tostring(L,-1);
		printf("call_on_each:%s\n", err);
  }
  else
  {
    ret=luaL_optinteger(L,-1,RET_OK);
  }
  lua_pop(L, 1);
  return ret;
}

static int wrap_widget_foreach(lua_State* L) {
  ret_t ret = 0;
  widget_t* widget = (widget_t*)tk_checkudata(L, 1, "widget_t");

  if (lua_isfunction(L, 2)) {
    struct tmpcalloneach_widaaa tmp;
    tmp.L=L;
    tmp.func=0;
    lua_pushvalue(L, 2);
    tmp.func = luaL_ref(L, LUA_REGISTRYINDEX);

    ret = (ret_t)widget_foreach(widget, call_on_each, &tmp);

    luaL_unref(L, LUA_REGISTRYINDEX, tmp.func);

    lua_pushnumber(L, (lua_Number)ret);

    return 1;
  } else {
    return 0;
  }
}

static int to_wstr(lua_State* L) {
  const char* str = (const char*)luaL_checkstring(L, 1);
  uint32_t size = (strlen(str) + 1) * sizeof(wchar_t);
  wchar_t* p = (wchar_t*)lua_newuserdata(L, size);

  tk_utf8_to_utf16(str, p, size);
  lua_pushlightuserdata(L, p);

  return 1;
}

static int to_str(lua_State* L) {
  const wchar_t* str = (const wchar_t*)lua_touserdata(L, 1);
  uint32_t size = (wcslen(str) + 1) * 3;
  char* p = (char*)lua_newuserdata(L, size);

  tk_utf8_from_utf16(str, p, size);
  lua_pushstring(L, p);

  return 1;
}

static ret_t timer_info_on_destroy(void* data) {
  timer_info_t* item = (timer_info_t*)data;
  lua_State* L = (lua_State*)item->on_destroy_ctx;

  //uint32_t func = (char*)(item->ctx) - (char*)NULL;
  //luaL_unref(L, LUA_REGISTRYINDEX, func);
  freeluacallfun((awtkluacb_base *)(item->ctx));

  return RET_OK;
}

static ret_t call_on_timer(const timer_info_t* timer) {
  ret_t ret = RET_OK;
  // lua_State* L = (lua_State*)s_current_L;
  // int func = (char*)(timer->ctx) - (char*)NULL;
  lua_State* L = ((awtkluacb_base *)timer->ctx)->loop_L;
  lua_settop(L, 0);
  //lua_rawgeti(L, LUA_REGISTRYINDEX, func);
  getluacallfun((awtkluacb_base *)timer->ctx);

  if (lua_pcall(L, 0, 1, 0)!=0)
	{
		const char* err=lua_tostring(L,-1);
		printf("call_on_timer:%s\n", err);
  }
  else
  {
    ret=luaL_optinteger(L,-1,RET_REMOVE);
  }
  lua_pop(L,1);

  return ret;
}

static int wrap_timer_add(lua_State* L) {
  uint32_t id = 0;
  if (lua_isfunction(L, 1)) {
    // int func = 0;
    uint32_t duration_ms = (uint32_t)luaL_checkinteger(L, 2);
    //lua_pushvalue(L, 1);
    //func = luaL_ref(L, LUA_REGISTRYINDEX);
    awtkluacb_base* func=pushluacallfun(L,1);

    id = timer_add(call_on_timer, func, duration_ms);
    timer_set_on_destroy(id, timer_info_on_destroy, L);
    lua_pushnumber(L, (lua_Number)id);

    return 1;
  } else {
    return 0;
  }
}

static ret_t idle_info_on_destroy(void* data) {
  idle_info_t* item = (idle_info_t*)data;
  lua_State* L = (lua_State*)item->on_destroy_ctx;

  //uint32_t func = (char*)(item->ctx) - (char*)NULL;
  //luaL_unref(L, LUA_REGISTRYINDEX, func);
  freeluacallfun((awtkluacb_base*)(item->ctx));

  return RET_OK;
}

static ret_t call_on_idle(const idle_info_t* idle) {
  ret_t ret = RET_OK;
  // lua_State* L = (lua_State*)s_current_L;
  // int func = (char*)(idle->ctx) - (char*)NULL;
  lua_State* L = ((awtkluacb_base *)idle->ctx)->loop_L;
  lua_settop(L, 0);
  //lua_rawgeti(L, LUA_REGISTRYINDEX, func);
  getluacallfun((awtkluacb_base *)idle->ctx);

  if (lua_pcall(L, 0, 1, 0)!=0)
	{
		const char* err=lua_tostring(L,-1);
		printf("call_on_idle:%s\n", err);
  }
  else
  {
    ret=luaL_optinteger(L,-1,RET_REMOVE);
  }
  lua_pop(L,1);
  return ret;
}

static int wrap_idle_add(lua_State* L) {
  uint32_t id = 0;
  if (lua_isfunction(L, 1)) {
    // int func = 0;
    // lua_pushvalue(L, 1);
    // func = luaL_ref(L, LUA_REGISTRYINDEX);
    awtkluacb_base* func=pushluacallfun(L,1);
    id = idle_add(call_on_idle, func);
    idle_set_on_destroy(id, idle_info_on_destroy, L);
    lua_pushnumber(L, (lua_Number)id);

    return 1;
  } else {
    return 0;
  }
}
