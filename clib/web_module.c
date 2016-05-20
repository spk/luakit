#ifdef WITH_WEBKIT2

#include "clib/web_module.h"
#include "common/tokenize.h"
#include "extension/msg.h"
#include "msg.h"

LUA_OBJECT_FUNCS(web_module_class, web_module_t, web_module);

#define luaH_check_web_module(L, idx) luaH_checkudata(L, idx, &(web_module_class))

GArray *module_refs;

static int
luaH_web_module_new(lua_State *L)
{
    const char *name = luaL_checkstring(L, -1);

    if (!name)
        return 0;

    lua_newtable(L);
    luaH_class_new(L, &web_module_class);
    web_module_t *web_module = luaH_check_web_module(L, -1);
    web_module->name = g_strdup(name);

    /* Append the reference to the module reference array */
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    g_array_append_val(module_refs, ref);
    web_module->module = module_refs->len - 1;

    msg_header_t header = {
        .type = MSG_TYPE_lua_require_module,
        .length = strlen(name)+1
    };
    msg_send(&header, name);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

    return 1;
}

static gint
luaH_web_module_gc(lua_State *L)
{
    web_module_t *web_module = luaH_check_web_module(L, -1);
    g_free(web_module->name);
    return luaH_object_gc(L);
}

static gint
web_module_send(lua_State *L)
{
    web_module_t *web_module = luaH_check_web_module(L, 1);
    luaL_checkstring(L, 2);

    GByteArray *buf = g_byte_array_new();

    g_byte_array_append(buf, (guint8*)&web_module->module, sizeof(web_module->module));
    lua_serialize_range(L, buf, 2, lua_gettop(L));

    msg_header_t header = {
        .type = MSG_TYPE_lua_msg,
        .length = buf->len
    };

    msg_send(&header, buf->data);
    g_byte_array_unref(buf);

    return 0;
}

void
web_module_recv(lua_State *L, const guint module, const gchar *arg, guint arglen)
{
    int ref = g_array_index(module_refs, int, module);
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    luaH_check_web_module(L, -1);

    int n = lua_deserialize_range(L, arg, arglen);
    const char *signame = lua_tostring(L, -n);
    lua_remove(L, -n);
    luaH_object_emit_signal(L, -n, signame, n-1, 0);
}

void
web_module_class_setup(lua_State *L)
{
    static const struct luaL_reg web_module_methods[] =
    {
        LUA_CLASS_METHODS(web_module)
        { "__call", luaH_web_module_new },
        { NULL, NULL }
    };

    static const struct luaL_reg web_module_meta[] =
    {
        LUA_OBJECT_META(web_module)
        { "emit_signal", web_module_send },
        { "__gc", luaH_web_module_gc },
        { NULL, NULL }
    };

    luaH_class_setup(L, &web_module_class, "web_module",
            (lua_class_allocator_t) web_module_new,
            NULL, NULL,
            web_module_methods, web_module_meta);

    module_refs = g_array_new(FALSE, FALSE, sizeof(int));
}

#endif

// vim: ft=c:et:sw=4:ts=8:sts=4:tw=80
