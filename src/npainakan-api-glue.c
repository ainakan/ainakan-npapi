#include <glib.h>
#include <glib-object.h>
#include <npainakan-object.h>
#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

static const gint npainakan_dispatcher_magic = 1337;

typedef struct _NPAinakanDispatcherInvocation NPAinakanDispatcherInvocation;

struct _NPAinakanDispatcherInvocation
{
  const gint * magic;

  volatile gint ref_count;

  GDBusMethodInfo * method;
  GVariant * parameters;
  GSimpleAsyncResult * res;

  GDBusMessage * call_message;
};

static void npainakan_dispatcher_unref (gpointer object);

static NPAinakanDispatcherInvocation * npainakan_dispatcher_invocation_new (GDBusMethodInfo * method, GVariant * parameters, GSimpleAsyncResult * res);
static void npainakan_dispatcher_invocation_ref (NPAinakanDispatcherInvocation * invocation);
static void npainakan_dispatcher_invocation_unref (NPAinakanDispatcherInvocation * invocation);
static void npainakan_dispatcher_invocation_perform (NPAinakanDispatcherInvocation * self);

static GDBusMessage * npainakan_dispatcher_invocation_get_message (GDBusMethodInvocation * invocation);
static GDBusConnection * npainakan_dispatcher_invocation_get_connection (GDBusMethodInvocation * invocation);
static gboolean npainakan_dispatcher_connection_send_message (GDBusConnection * connection, GDBusMessage * message, GDBusSendMessageFlags flags, volatile guint32 * out_serial, GError ** error);
static void npainakan_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error);

#define g_object_unref(obj) \
    npainakan_dispatcher_unref (obj)
#define g_dbus_method_invocation_get_message(invocation) \
    npainakan_dispatcher_invocation_get_message (invocation)
#define g_dbus_method_invocation_get_connection(invocation) \
    npainakan_dispatcher_invocation_get_connection (invocation)
#define g_dbus_connection_send_message(connection, message, flags, out_serial, error) \
    npainakan_dispatcher_connection_send_message (connection, message, flags, out_serial, error)
#define g_dbus_method_invocation_return_gerror(invocation, error) \
    npainakan_dispatcher_invocation_return_gerror (invocation, error)

#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wincompatible-pointer-types"
# pragma clang diagnostic ignored "-Wunused-variable"
# pragma clang diagnostic ignored "-Wunused-value"
#endif
#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4054 4055 4090 4100 4152 4189 4267 4702)
#endif
#include "npainakan-api.c"
#ifdef _MSC_VER
# pragma warning (pop)
#endif
#ifdef __clang__
# pragma clang diagnostic pop
#endif

#undef g_object_unref
#undef g_dbus_method_invocation_get_message
#undef g_dbus_method_invocation_get_connection
#undef g_dbus_connection_send_message
#undef g_dbus_method_invocation_return_gerror

void
npainakan_dispatcher_init_with_object (NPAinakanDispatcher * self, NPAinakanObject * obj)
{
  GType type;

  self->target_object = obj;

  type = G_TYPE_FROM_INSTANCE (obj);

  if (g_type_is_a (type, NPAINAKAN_TYPE_ROOT_API))
  {
    self->methods = (GDBusMethodInfo **) _npainakan_root_api_dbus_method_info;
    self->dispatch_func = npainakan_root_api_dbus_interface_method_call;
  }
  else
    g_assert_not_reached ();
}

static void
npainakan_dispatcher_do_invoke (NPAinakanDispatcher * self, GDBusMethodInfo * method, GVariant * parameters,
    GAsyncReadyCallback callback, gpointer user_data)
{
  NPAinakanDispatcherInvocation * invocation;

  invocation = npainakan_dispatcher_invocation_new (method, parameters,
      g_simple_async_result_new (G_OBJECT (self), callback, user_data, GSIZE_TO_POINTER (npainakan_dispatcher_do_invoke)));
  npainakan_dispatcher_invocation_perform (invocation);
  npainakan_dispatcher_invocation_unref (invocation);
}

static GVariant *
npainakan_dispatcher_do_invoke_finish (NPAinakanDispatcher * self, GAsyncResult * res, GError ** error)
{
  GSimpleAsyncResult * ar = G_SIMPLE_ASYNC_RESULT (res);

  (void) self;

  if (g_simple_async_result_propagate_error (ar, error))
    return NULL;

  return g_variant_ref (g_simple_async_result_get_op_res_gpointer (ar));
}

static void
npainakan_dispatcher_unref (gpointer object)
{
  NPAinakanDispatcherInvocation * invocation = object;
  if (invocation->magic == &npainakan_dispatcher_magic)
    npainakan_dispatcher_invocation_unref (invocation);
  else
    g_object_unref (object);
}

static NPAinakanDispatcherInvocation *
npainakan_dispatcher_invocation_new (GDBusMethodInfo * method, GVariant * parameters, GSimpleAsyncResult * res)
{
  NPAinakanDispatcherInvocation * invocation;

  invocation = g_slice_new (NPAinakanDispatcherInvocation);
  invocation->magic = &npainakan_dispatcher_magic;

  invocation->ref_count = 1;

  invocation->method = g_dbus_method_info_ref (method);
  invocation->parameters = g_variant_ref (parameters);
  invocation->res = res;

  invocation->call_message = g_dbus_message_new_method_call (NULL, "/org/boblycat/ainakan/Foo", "org.boblycat.Ainakan.Foo", method->name);
  g_dbus_message_set_serial (invocation->call_message, 1);

  return invocation;
}

static void
npainakan_dispatcher_invocation_ref (NPAinakanDispatcherInvocation * invocation)
{
  g_atomic_int_inc (&invocation->ref_count);
}

static void
npainakan_dispatcher_invocation_unref (NPAinakanDispatcherInvocation * invocation)
{
  if (g_atomic_int_dec_and_test (&invocation->ref_count))
  {
    g_dbus_method_info_unref (invocation->method);
    g_variant_unref (invocation->parameters);
    g_object_unref (invocation->res);

    g_object_unref (invocation->call_message);

    g_slice_free (NPAinakanDispatcherInvocation, invocation);
  }
}

static void
npainakan_dispatcher_invocation_perform (NPAinakanDispatcherInvocation * self)
{
  NPAinakanDispatcher * dispatcher;

  dispatcher = NPAINAKAN_DISPATCHER (g_async_result_get_source_object (G_ASYNC_RESULT (self->res)));

  npainakan_dispatcher_invocation_ref (self);
  dispatcher->dispatch_func (NULL, NULL, NULL, NULL, self->method->name, self->parameters,
      (GDBusMethodInvocation *) self, &dispatcher->target_object);

  g_object_unref (dispatcher);
}

static GDBusMessage *
npainakan_dispatcher_invocation_get_message (GDBusMethodInvocation * invocation)
{
  return ((NPAinakanDispatcherInvocation *) invocation)->call_message;
}

static GDBusConnection *
npainakan_dispatcher_invocation_get_connection (GDBusMethodInvocation * invocation)
{
  return (GDBusConnection *) invocation;
}

static gboolean
npainakan_dispatcher_connection_send_message (GDBusConnection * connection, GDBusMessage * message, GDBusSendMessageFlags flags, volatile guint32 * out_serial, GError ** error)
{
  NPAinakanDispatcherInvocation * self = (NPAinakanDispatcherInvocation *) connection;
  GVariant * result;

  (void) flags;
  (void) out_serial;
  (void) error;

  result = g_variant_ref (g_dbus_message_get_body (message));
  g_simple_async_result_set_op_res_gpointer (self->res, result, (GDestroyNotify) g_variant_unref);
  g_simple_async_result_complete (self->res);

  return TRUE;
}

static void
npainakan_dispatcher_invocation_return_gerror (GDBusMethodInvocation * invocation, const GError * error)
{
  NPAinakanDispatcherInvocation * self = (NPAinakanDispatcherInvocation *) invocation;

  g_simple_async_result_take_error (self->res, (GError *) error);
  g_simple_async_result_complete (self->res);

  npainakan_dispatcher_invocation_unref (self);
}
