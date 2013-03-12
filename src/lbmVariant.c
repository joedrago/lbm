#include "lbmVariant.h"

#include "lua.h"
#include "lstate.h"

#include <stdlib.h>
#include <stdio.h>

int lua_absindex (lua_State *L, int idx)
{
    return (idx > 0 || idx <= LUA_REGISTRYINDEX)
        ? idx
        : cast_int(L->top - L->ci->func + idx);
}

lbmVariant *lbmVariantCreate(int type)
{
    lbmVariant *arg = calloc(1, sizeof(lbmVariant));
    arg->type = type;
    if(arg->type == V_MAP)
    {
        arg->m = dmCreate(DKF_STRING, 0);
    }
    return arg;
}

void lbmVariantDestroy(lbmVariant *arg)
{
    lbmVariantClear(arg);
    free(arg);
}

void lbmVariantClear(lbmVariant *arg)
{
    switch (arg->type)
    {
        case V_STRING:
            dsDestroy(&arg->s);
            break;
        case V_ARRAY:
            daDestroy(&arg->a, lbmVariantDestroy);
            break;
        case V_MAP:
            dmDestroy(arg->m, lbmVariantDestroy);
            break;
    };
}

#define PRINTDEPTH(DEPTH) { int d; for(d = 0; d < (DEPTH); ++d) printf("  "); }

static int lbmVariantPrintMap(dynMap *dm, dynMapEntry *e, void *userData)
{
    int *depth = (int *)userData;
    lbmVariant *v = dmEntryDefaultData(e)->valuePtr;
    PRINTDEPTH(*depth + 1);
    printf("* key: %s\n", e->keyStr);
    lbmVariantPrint(v, *depth + 2);
    return 1;
}

void lbmVariantPrint(lbmVariant *v, int depth)
{
    int i;
    switch (v->type)
    {
        case V_NONE:
            PRINTDEPTH(depth);
            printf("* nil\n");
            break;
        case V_STRING:
            PRINTDEPTH(depth);
            printf("* string: %s\n", v->s);
            break;
        case V_ARRAY:
            PRINTDEPTH(depth);
            printf("* array: %d element(s)\n", (int)daSize(&v->a));
            for(i = 0; i < daSize(&v->a); ++i)
            {
                lbmVariantPrint(v->a[i], depth + 1);
            }
            break;
        case V_MAP:
            PRINTDEPTH(depth);
            printf("* map: %d element(s)\n", v->m->count);
            dmIterate(v->m, lbmVariantPrintMap, &depth);
            break;
    };
}

lbmVariant *lbmVariantFromIndex(lua_State *L, int index)
{
    lbmVariant *ret = NULL;
    lbmVariant *child;
    const char *s = NULL;
    size_t l;
    char temp[16];

    int type = lua_type(L, index);

    switch (type)
    {
        // Unimplemented:
        // LUA_TNIL
        // LUA_TLIGHTUSERDATA
        // LUA_TTABLE
        // LUA_TFUNCTION
        // LUA_TUSERDATA
        // LUA_TTHREAD

        case LUA_TBOOLEAN:
            ret = lbmVariantCreate(V_STRING);
            s = (lua_toboolean(L, index)) ? "true" : "false";
            dsCopy(&ret->s, s);
            break;
        case LUA_TNUMBER:
            ret = lbmVariantCreate(V_STRING);
            sprintf(temp, "%d", (int)lua_tonumber(L, index));
            dsCopy(&ret->s, temp);
            break;
        case LUA_TSTRING:
            ret = lbmVariantCreate(V_STRING);
            s = lua_tolstring(L, index, &l);
            dsCopy(&ret->s, s);
            break;
        case LUA_TTABLE:
            lua_pushnil(L);  /* first key */
            while (lua_next(L, index))
            {
                // uses 'key' (at index -2) and 'value' (at index -1)

                if(ret == NULL)
                {
                    // Lazily create this so a table can either be a map or an array
                    if(lua_type(L, -2) == LUA_TSTRING)
                    {
                        ret = lbmVariantCreate(V_MAP);
                    }
                    else
                    {
                        ret = lbmVariantCreate(V_ARRAY);
                    }
                }

                if(ret->type == V_MAP)
                {
                    if(lua_type(L, -2) == LUA_TSTRING)
                    {
                        s = lua_tolstring(L, -2, &l);
                    }
                    else if(lua_type(L, -2) == LUA_TNUMBER)
                    {
                        sprintf(temp, "%d", (int)lua_tonumber(L, -2));
                        s = temp;
                    }

                    if(s != NULL)
                    {
                        child = lbmVariantFromIndex(L, lua_absindex(L, -1));
                        dmGetS2P(ret->m, s) = child;
                    }
                }
                else
                {
                    child = lbmVariantFromIndex(L, lua_absindex(L, -1));
                    daPush(&ret->a, child);
                }

                // removes 'value'; keeps 'key' for next iteration
                lua_pop(L, 1);
            }

            break;
    };

    if(!ret)
    {
        ret = lbmVariantCreate(V_NONE);
    }
    return ret;
}

lbmVariant *lbmVariantFromArgs(lua_State *L)
{
    int argCount = lua_gettop(L);
    int i;
    lbmVariant *ret = lbmVariantCreate(V_ARRAY);
    lbmVariant *child;

    for (i=1; i <= argCount; ++i)
    {
        child = lbmVariantFromIndex(L, i);
        daPush(&ret->a, child);
    }
    return ret;
}
