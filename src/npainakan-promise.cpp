#include "npainakan-promise.h"

#include "npainakan-plugin.h"

#include <string.h>

#define NPAINAKAN_PROMISE_LOCK() \
    g_mutex_lock (&self->mutex)
#define NPAINAKAN_PROMISE_UNLOCK() \
    g_mutex_unlock (&self->mutex)

static void npainakan_promise_deliver (NPAinakanPromise * self, NPAinakanPromiseResult result, const NPVariant * args, guint arg_count);
static void npainakan_promise_flush (void * data);
static void npainakan_promise_flush_unlocked (NPAinakanPromise * self);
static void npainakan_promise_invoke_callback (gpointer data, gpointer user_data);

static NPObject *
npainakan_promise_allocate (NPP npp, NPClass * klass)
{
  NPAinakanPromise * promise;

  (void) klass;

  promise = g_slice_new0 (NPAinakanPromise);
  promise->npp = npp;

  g_mutex_init (&promise->mutex);

  promise->result = NPAINAKAN_PROMISE_PENDING;
  promise->args = g_array_new (FALSE, FALSE, sizeof (NPVariant));

  promise->on_success = g_ptr_array_new_with_free_func (npainakan_npobject_release);
  promise->on_failure = g_ptr_array_new_with_free_func (npainakan_npobject_release);
  promise->on_complete = g_ptr_array_new_with_free_func (npainakan_npobject_release);

  return &promise->np_object;
}

static void
npainakan_promise_deallocate (NPObject * npobj)
{
  NPAinakanPromise * promise = reinterpret_cast<NPAinakanPromise *> (npobj);
  guint i;

  if (promise->destroy_user_data != NULL)
    promise->destroy_user_data (promise->user_data);

  g_mutex_clear (&promise->mutex);

  for (i = 0; i != promise->args->len; i++)
    npainakan_nsfuncs->releasevariantvalue (&g_array_index (promise->args, NPVariant, i));
  g_array_unref (promise->args);

  g_ptr_array_unref (promise->on_success);
  g_ptr_array_unref (promise->on_failure);
  g_ptr_array_unref (promise->on_complete);

  g_slice_free (NPAinakanPromise, promise);
}

static void
npainakan_promise_invalidate (NPObject * npobj)
{
  (void) npobj;
}

void
npainakan_promise_resolve (NPAinakanPromise * self, const NPVariant * args, guint arg_count)
{
  npainakan_promise_deliver (self, NPAINAKAN_PROMISE_SUCCESS, args, arg_count);
}

void
npainakan_promise_reject (NPAinakanPromise * self, const NPVariant * args, guint arg_count)
{
  npainakan_promise_deliver (self, NPAINAKAN_PROMISE_FAILURE, args, arg_count);
}

static void
npainakan_promise_deliver (NPAinakanPromise * self, NPAinakanPromiseResult result, const NPVariant * args, guint arg_count)
{
  guint i;

  NPAINAKAN_PROMISE_LOCK ();

  g_assert (self->result == NPAINAKAN_PROMISE_PENDING);
  self->result = result;

  g_array_set_size (self->args, arg_count);
  for (i = 0; i != arg_count; i++)
    npainakan_init_npvariant_with_other (&g_array_index (self->args, NPVariant, i), &args[i]);

  NPAINAKAN_PROMISE_UNLOCK ();

  npainakan_nsfuncs->retainobject (&self->np_object);
  npainakan_nsfuncs->pluginthreadasynccall (self->npp, npainakan_promise_flush, self);
}

static void
npainakan_promise_flush (void * data)
{
  NPAinakanPromise * self = static_cast<NPAinakanPromise *> (data);

  NPAINAKAN_PROMISE_LOCK ();
  npainakan_promise_flush_unlocked (self);
  NPAINAKAN_PROMISE_UNLOCK ();

  npainakan_nsfuncs->releaseobject (&self->np_object);
}

static void
npainakan_promise_flush_unlocked (NPAinakanPromise * self)
{
  GPtrArray * on_success, * on_failure, * on_complete;

  if (self->result == NPAINAKAN_PROMISE_PENDING)
    return;

  on_success = self->on_success; self->on_success = g_ptr_array_new_with_free_func (npainakan_npobject_release);
  on_failure = self->on_failure; self->on_failure = g_ptr_array_new_with_free_func (npainakan_npobject_release);
  on_complete = self->on_complete; self->on_complete = g_ptr_array_new_with_free_func (npainakan_npobject_release);

  NPAINAKAN_PROMISE_UNLOCK ();

  if (self->result == NPAINAKAN_PROMISE_SUCCESS)
    g_ptr_array_foreach (on_success, npainakan_promise_invoke_callback, self);
  else
    g_ptr_array_foreach (on_failure, npainakan_promise_invoke_callback, self);
  g_ptr_array_foreach (on_complete, npainakan_promise_invoke_callback, self);

  g_ptr_array_unref (on_success);
  g_ptr_array_unref (on_failure);
  g_ptr_array_unref (on_complete);

  NPAINAKAN_PROMISE_LOCK ();
}

static void
npainakan_promise_invoke_callback (gpointer data, gpointer user_data)
{
  NPAinakanPromise * self = static_cast<NPAinakanPromise *> (user_data);
  NPObject * callback = static_cast<NPObject *> (data);
  NPVariant result;

  VOID_TO_NPVARIANT (result);
  npainakan_nsfuncs->invokeDefault (self->npp, callback, &g_array_index (self->args, NPVariant, 0), self->args->len, &result);
  npainakan_nsfuncs->releasevariantvalue (&result);
}

static bool
npainakan_promise_has_method (NPObject * npobj, NPIdentifier name)
{
  const gchar * function_name = static_cast<NPString *> (name)->UTF8Characters;

  (void) npobj;

  return (strcmp (function_name, "always") == 0 ||
      strcmp (function_name, "done") == 0 ||
      strcmp (function_name, "fail") == 0 ||
      strcmp (function_name, "state") == 0);
}

static bool
npainakan_promise_invoke (NPObject * npobj, NPIdentifier name, const NPVariant * args, uint32_t arg_count, NPVariant * result)
{
  NPAinakanPromise * self = reinterpret_cast<NPAinakanPromise *> (npobj);
  const gchar * function_name = static_cast<NPString *> (name)->UTF8Characters;
  NPObject * callback;
  GPtrArray * callbacks;

  if (strcmp (function_name, "state") == 0)
  {
    const gchar * state = NULL;

    if (arg_count != 0)
    {
      npainakan_nsfuncs->setexception (npobj, "invalid argument");
      return true;
    }

    switch (self->result)
    {
      case NPAINAKAN_PROMISE_PENDING:
        state = "pending";
        break;
      case NPAINAKAN_PROMISE_SUCCESS:
        state = "resolved";
        break;
      case NPAINAKAN_PROMISE_FAILURE:
        state = "rejected";
        break;
    }

    npainakan_init_npvariant_with_string (result, state);
  }
  else
  {
    if (arg_count != 1 || args[0].type != NPVariantType_Object)
    {
      npainakan_nsfuncs->setexception (npobj, "invalid argument");
      return true;
    }

    callback = npainakan_nsfuncs->retainobject (NPVARIANT_TO_OBJECT (args[0]));

    NPAINAKAN_PROMISE_LOCK ();
    if (strcmp (function_name, "done") == 0)
      callbacks = self->on_success;
    else if (strcmp (function_name, "fail") == 0)
      callbacks = self->on_failure;
    else if (strcmp (function_name, "always") == 0)
      callbacks = self->on_complete;
    else
      callbacks = NULL;
    g_assert (callbacks != NULL);

    g_ptr_array_add (callbacks, callback);
    npainakan_promise_flush_unlocked (self);
    NPAINAKAN_PROMISE_UNLOCK ();

    OBJECT_TO_NPVARIANT (npainakan_nsfuncs->retainobject (npobj), *result);
  }

  return true;
}

static NPClass npainakan_promise_np_class =
{
  NP_CLASS_STRUCT_VERSION,
  npainakan_promise_allocate,
  npainakan_promise_deallocate,
  npainakan_promise_invalidate,
  npainakan_promise_has_method,
  npainakan_promise_invoke,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

NPObject *
npainakan_promise_new (NPP npp, gpointer user_data, GDestroyNotify destroy_user_data)
{
  NPAinakanPromise * promise;

  promise = reinterpret_cast<NPAinakanPromise *> (npainakan_nsfuncs->createobject (npp, &npainakan_promise_np_class));
  promise->user_data = user_data;
  promise->destroy_user_data = destroy_user_data;

  return &promise->np_object;
}
