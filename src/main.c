#include "dyn.h"
#include "lbmVariant.h"
#include "lbmRenderer.h"
#include "lbmBaseLua.h"

#include "lua.h"
#include "lstate.h"
#include "lualib.h"
#include "lauxlib.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>   // for GetCurrentDirectory()
#else
#include <unistd.h>    // for getcwd()
#include <sys/types.h> // for S_* defines
#include <sys/stat.h>  // for mkdir()
#include <dirent.h>    // for opendir()
#endif

// ---------------------------------------------------------------------------
// Path helpers

const char * lbmWorkingDir()
{
#ifdef WIN32
    static char currentDir[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, currentDir);
    return currentDir;
#else
    static char cwd[512];
    return getcwd(cwd, 512);
#endif
}

int lbmDirExists(const char * path)
{
    int ret = 0;

#ifdef WIN32
    DWORD dwAttrib = GetFileAttributes(path);
    if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
    {
        ret = 1;
    }
#else
    DIR * p = opendir(path);
    if (p != NULL)
    {
        ret = 1;
        closedir(p);
    }
#endif

    return ret;
}

void lbmMkdir(const char * path)
{
    if (!lbmDirExists(path))
    {
        printf("Creating directory: %s\n", path);

#ifdef WIN32
        CreateDirectory(path, NULL);
#else
        mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

    }
}

// ---------------------------------------------------------------------------
// File reading

char * lbmFileAlloc(const char * filename, int * outputLen)
{
    char * data;
    int len;
    int bytesRead;

    FILE * f = fopen(filename, "rb");
    if (!f)
    {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    len = (int)ftell(f);
    if (len < 1)
    {
        fclose(f);
        return NULL;
    }

    fseek(f, 0, SEEK_SET);
    data = (char *)calloc(1, len + 1);
    bytesRead = fread(data, 1, len, f);
    if (bytesRead != len)
    {
        free(data);
        data = NULL;
    }

    fclose(f);
    *outputLen = len;
    return data;
}

// ---------------------------------------------------------------------------
// Script loading

struct lbmScriptInfo
{
    const char * script;
    int len;
};

static const char * lbmLoadScriptReader(lua_State * L, void * data, size_t * size)
{
    struct lbmScriptInfo * info = (struct lbmScriptInfo *)data;
    if (info->script && info->len)
    {
        *size = info->len;
        info->len = 0;
        return info->script;
    }
    return NULL;
}

static int lbmLoadScript(lua_State * L, const char * name, const char * script, int len)
{
    int err;
    struct lbmScriptInfo info;
    info.script = script;
    info.len = len;
    err = lua_load(L, lbmLoadScriptReader, &info, name);
    if (err == 0)
    {
        err = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (err == 0)
        {
            return 1;
        }
        else
        {
            // failed to run chunk
            printf("ERROR: %s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
    else
    {
        // failed to load chunk
        printf("ERROR: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

// ---------------------------------------------------------------------------

#ifdef WIN32
#define PROPER_SLASH '\\'
#define PROPER_CURRENT "\\."
#define PROPER_PARENT "\\.."
#else
#define PROPER_SLASH '/'
#define PROPER_CURRENT "/."
#define PROPER_PARENT "/.."
#endif

static int isAbsolutePath(const char * s)
{
#ifdef WIN32
    if ((strlen(s) >= 3) && (s[1] == ':') && (s[2] == PROPER_SLASH))
    {
        char driveLetter = tolower(s[0]);
        if ((driveLetter >= 'a') && (driveLetter <= 'z'))
        {
            return 1;
        }
    }
#endif
    if (s[0] == PROPER_SLASH)
    {
        return 1;
    }
    return 0;
}

static void dsCleanupSlashes(char ** ds)
{
    char * c;
    char * head;
    char * tail;
    int wasSlash = 0;
    if (!dsLength(ds))
    {
        return;
    }

    // Remove all trailing slashes
    c = *ds + (dsLength(ds) - 1);
    for (; c >= *ds; --c)
    {
        if ((*c == '/') || (*c == '\\'))
        {
            *c = 0;
        }
        else
        {
            break;
        }
    }

    // Remove all duplicate slashes, fix slash direction
    head = *ds;
    tail = *ds;
    for (; *tail; ++tail)
    {
        int isSlash = ((*tail == '/') || (*tail == '\\')) ? 1 : 0;
        if (isSlash)
        {
            if (!wasSlash)
            {
                *head = PROPER_SLASH;
                ++head;
            }
        }
        else
        {
            *head = *tail;
            ++head;
        }
        wasSlash = isSlash;
    }
    *head = 0;

    // Clue in the dynString that it was manually altered
    dsCalcLength(ds);
}

static void dsSquashDotDirs(char ** ds)
{
    char * dot;
    char * prevSlash;
    while ((dot = strstr(*ds, PROPER_PARENT)) != NULL)
    {
        if (dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot - 1;
        dot += strlen(PROPER_PARENT);
        while ((prevSlash != *ds) && (*prevSlash != PROPER_SLASH))
        {
            --prevSlash;
        }
        if (prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    while ((dot = strstr(*ds, PROPER_CURRENT)) != NULL)
    {
        if (dot == *ds)
        {
            // bad path
            return;
        }
        prevSlash = dot;
        dot += strlen(PROPER_CURRENT);
        if (prevSlash != *ds)
        {
            memmove(prevSlash, dot, strlen(dot)+1);
        }
    }
    dsCalcLength(ds);
}

void lbmCanonicalizePath(char ** dspath, const char * curDir)
{
    char * temp = NULL;
    if (!curDir || !strlen(curDir))
    {
        curDir = ".";
    }

    if (isAbsolutePath(*dspath))
    {
        dsCopy(&temp, *dspath);
    }
    else
    {
        dsPrintf(&temp, "%s/%s", curDir, *dspath);
    }

    dsCleanupSlashes(&temp);
    dsSquashDotDirs(&temp);

    dsCopy(dspath, temp);
    dsDestroy(&temp);
}

static const char * product(dynMap * vars, const char * in, char ** out, char end)
{
    char * leftovers = NULL;
    char * varname = NULL;
    const char * c = in;
    const char * varval;

    int uppercase;
    const char * colon;
    const char * nextBackslash;
    const char * nextOpenBrace;
    const char * nextCloseBrace;

    dsCopy(&leftovers, "");
    for (; *c && *c != end; ++c)
    {
        if (*c == '{')
        {
            // Append any additional text we've seen to each entry in list we have so far
            dsConcat(out, leftovers);
            dsCopy(&leftovers, "");

            ++c;

            uppercase = 0;
            colon = strchr(c, ':');
            nextBackslash = strchr(c, '\\');
            nextOpenBrace = strchr(c, '{');
            nextCloseBrace = strchr(c, '}');
            if (colon
                && (!nextBackslash  || colon < nextBackslash)
                && (!nextOpenBrace  || colon < nextOpenBrace)
                && (!nextCloseBrace || colon < nextCloseBrace))
            {
                for (; c != colon; ++c)
                {
                    switch (*c)
                    {
                        case 'u':
                            uppercase = 1;
                            break;
                    }
                }
                ++c;
            }

            // Now look up all variables contained in the {}
            c = product(vars, c, &varname, '}');
            varval = dmGetS2P(vars, varname);
            if (varval)
            {
                dsConcat(out, varval);
            }

            dsDestroy(&varname);
        }
        else
        {
            // Just append
            dsConcatLen(&leftovers, c, 1);
        }
    }

    // If there was stuff trailing at the end, be sure to grab it
    dsConcat(out, leftovers);
    dsDestroy(&leftovers);
    return c;
}

// ---------------------------------------------------------------------------
// Lua lbm functions

int lbm_canonicalize(lua_State * L, struct lbmVariant * args)
{
    char * temp = NULL;
    dsCopy(&temp, args->a[0]->s);
    lbmCanonicalizePath(&temp, args->a[1]->s);
    lua_pushstring(L, temp);
    dsDestroy(&temp);
    return 1;
}

int lbm_interp(lua_State * L, struct lbmVariant * args)
{
    // TODO: error checking of any kind
    const char * template = args->a[0]->s;
    char * out = NULL;
    char * basename = NULL;
    dynMap * argMap = args->a[1]->m;
    dynMap * vars = dmCreate(DKF_STRING, 0);

    if (dmHasS(argMap, "path"))
    {
        lbmVariant * variant = dmGetS2P(argMap, "path");
        const char * path = variant->s;
        if (path)
        {
            char * dotLoc;
            char * slashLoc1;
            char * slashLoc2;
            char * basenameLoc;
            dsCopy(&basename, path);
            dotLoc = strrchr(basename, '.');
            slashLoc1 = strrchr(basename, '/');
            slashLoc2 = strrchr(basename, '\\');
            if (!slashLoc1)
            {
                slashLoc1 = slashLoc2;
            }
            if (slashLoc2)
            {
                if (slashLoc1 < slashLoc2)
                {
                    slashLoc1 = slashLoc2;
                }
            }
            if (dotLoc)
            {
                *dotLoc = 0;
            }
            if (slashLoc1)
            {
                basenameLoc = slashLoc1 + 1;
            }
            else
            {
                basenameLoc = basename;
            }
            dmGetS2P(vars, "BASENAME") = basenameLoc;
        }
    }

    product(vars, template, &out, 0);

    if (dmHasS(argMap, "root"))
    {
        lbmVariant * variant = dmGetS2P(argMap, "root");
        const char * root = variant->s;
        if (root)
        {
            lbmCanonicalizePath(&out, root);
        }
    }

    lua_pushstring(L, out);
    dsDestroy(&out);
    dsDestroy(&basename);
    return 1;
}

int lbm_die(lua_State * L, struct lbmVariant * args)
{
    const char * error = "<unknown>";
    if (args->a[0]->s)
    {
        error = args->a[0]->s;
    }
    luaL_error(L, error);
    exit(-1);
    return 0;
}

int lbm_read(lua_State * L, struct lbmVariant * args)
{
    int ret = 0;
    int len = 0;
    char * text = lbmFileAlloc(args->a[0]->s, &len);
    if (text)
    {
        if (len)
        {
            lua_pushstring(L, text);
            ++ret;
        }
        free(text);
    }
    return ret;
}

int lbm_write(lua_State * L, struct lbmVariant * args)
{
    char * err = NULL;
    const char * filename = args->a[0]->s;
    const char * text = args->a[1]->s;
    FILE * f = fopen(filename, "wb");
    dsCopy(&err, "");
    if (f)
    {
        int len = strlen(text);
        fwrite(text, 1, len, f);
        fclose(f);
    }
    lua_pushstring(L, err);
    dsDestroy(&err);
    return 1;
}

int lbm_mkdir_for_file(lua_State * L, struct lbmVariant * args)
{
    const char * path = args->a[0]->s;
    char * dirpath = NULL;
    char * slashLoc1;
    char * slashLoc2;
    dsCopy(&dirpath, path);
    slashLoc1 = strrchr(dirpath, '/');
    slashLoc2 = strrchr(dirpath, '\\');
    if (!slashLoc1)
    {
        slashLoc1 = slashLoc2;
    }
    if (slashLoc2)
    {
        if (slashLoc1 < slashLoc2)
        {
            slashLoc1 = slashLoc2;
        }
    }
    if (slashLoc1)
    {
        *slashLoc1 = 0;
        lbmMkdir(dirpath);
    }
    dsDestroy(&dirpath);
    return 0;
}

// ---------------------------------------------------------------------------
// Lua lbm function hooks

static int unimplemented(lua_State * L)
{
    lbmVariant * variant = lbmVariantFromArgs(L);
    printf("UNIMPLEMENTED func called. Args:\n");
    lbmVariantPrint(variant, 1);
    lbmVariantDestroy(variant);
    return 0;
}

#define LUA_CONTEXT_DECLARE_STUB(NAME) { #NAME, unimplemented }
#define LUA_CONTEXT_DECLARE_FUNC(NAME) { #NAME, LuaFunc_ ## NAME }
#define LUA_CONTEXT_IMPLEMENT_FUNC(NAME, CONTEXTFUNC) \
    static int LuaFunc_ ## NAME (lua_State *L) \
    { \
        int ret; \
        lbmVariant *args = lbmVariantFromArgs(L); \
        ret = CONTEXTFUNC(L, args); \
        lbmVariantDestroy(args); \
        return ret; \
    }

LUA_CONTEXT_IMPLEMENT_FUNC(interp, lbm_interp);
LUA_CONTEXT_IMPLEMENT_FUNC(canonicalize, lbm_canonicalize);
LUA_CONTEXT_IMPLEMENT_FUNC(die, lbm_die);
LUA_CONTEXT_IMPLEMENT_FUNC(read, lbm_read);
LUA_CONTEXT_IMPLEMENT_FUNC(write, lbm_write);
LUA_CONTEXT_IMPLEMENT_FUNC(mkdir_for_file, lbm_mkdir_for_file);

static const luaL_Reg lbmFuncs[] =
{
    LUA_CONTEXT_DECLARE_FUNC(interp),
    LUA_CONTEXT_DECLARE_FUNC(canonicalize),
    LUA_CONTEXT_DECLARE_FUNC(die),
    LUA_CONTEXT_DECLARE_FUNC(read),
    LUA_CONTEXT_DECLARE_FUNC(write),
    LUA_CONTEXT_DECLARE_FUNC(mkdir_for_file),
    {NULL, NULL}
};

static void lbmPrepareLua(lua_State * L, int argc, char ** argv)
{
    int isUNIX = 0;
    int isWIN32 = 0;

#ifdef __unix__
    isUNIX = 1;
#endif

#ifdef WIN32
    isWIN32 = 1;
#endif

    luaL_openlibs(L);

    luaL_register(L, "lbm", lbmFuncs);

    lua_pushboolean(L, isUNIX);
    lua_setfield(L, -2, "unix");
    lua_pushboolean(L, isWIN32);
    lua_setfield(L, -2, "win32");
    lua_pushstring(L, lbmWorkingDir());
    lua_setfield(L, -2, "cwd");
    lua_pushstring(L, argv[0]);
    lua_setfield(L, -2, "cmd");

    lua_newtable(L);
    {
        int i;
        for (i = 1; i < argc; ++i)
        {
            lua_pushinteger(L, i);
            lua_pushstring(L, argv[i]);
            lua_settable(L, -3);
        }
    }
    lua_setfield(L, -2, "args");

    lua_pop(L, 1); // pop "lbm"
    lbmLoadScript(L, "lbmBase", lbmBaseLuaData, lbmBaseLuaSize);
}

// ---------------------------------------------------------------------------
// Main

int main(int argc, char ** argv)
{
    lua_State * L = luaL_newstate();
    lbmRenderer *renderer;

    lbmRendererStartup();
    lbmPrepareLua(L, argc, argv);
    renderer = lbmRendererCreate();
    lbmPump();
    lbmRendererShutdown();
    return 0;
}
