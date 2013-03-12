#ifndef DCMVARIANT_H
#define DCMVARIANT_H

#include "dyn.h"

struct lua_State;

enum
{
    V_NONE = 0, // 'n': nil / absent
    V_STRING,   // 's': must be a basic string
    V_ARRAY,    // 'a'
    V_MAP       // 'm'
};

struct lbmVariant;

typedef struct lbmVariant
{
    int type;
    union
    {
        char *s;
        struct lbmVariant **a;
        dynMap *m;
    };
} lbmVariant;

lbmVariant *lbmVariantCreate(int type);
void lbmVariantDestroy(lbmVariant *arg);
void lbmVariantClear(lbmVariant *arg);
void lbmVariantPrint(lbmVariant *v, int depth); // for debugging
lbmVariant *lbmVariantFromArgs(struct lua_State *L);
lbmVariant *lbmVariantFromIndex(struct lua_State *L, int index);

#endif
