#ifndef __NPAINAKAN_PROMISE_H__
#define __NPAINAKAN_PROMISE_H__

#include "npainakan-plugin.h"

typedef struct _NPAinakanPromise NPAinakanPromise;
typedef gint NPAinakanPromiseResult;

enum _NPAinakanPromiseResult
{
  NPAINAKAN_PROMISE_PENDING,
  NPAINAKAN_PROMISE_SUCCESS,
  NPAINAKAN_PROMISE_FAILURE
};

struct _NPAinakanPromise
{
  NPObject np_object;

  gpointer user_data;
  GDestroyNotify destroy_user_data;

  /*< private */
  NPP npp;
  GMutex mutex;
  NPAinakanPromiseResult result;
  GArray * args;
  GPtrArray * on_success;
  GPtrArray * on_failure;
  GPtrArray * on_complete;
};

NPObject * npainakan_promise_new (NPP npp, gpointer user_data, GDestroyNotify destroy_user_data);

void npainakan_promise_resolve (NPAinakanPromise * self, const NPVariant * args, guint arg_count);
void npainakan_promise_reject (NPAinakanPromise * self, const NPVariant * args, guint arg_count);

#endif
