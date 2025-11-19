#ifndef __NPAINAKAN_OBJECT_H__
#define __NPAINAKAN_OBJECT_H__

#include <glib-object.h>
#include <gio/gio.h>

#define NPAINAKAN_TYPE_OBJECT (npainakan_object_get_type ())
#define NPAINAKAN_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NPAINAKAN_TYPE_OBJECT, NPAinakanObject))
#define NPAINAKAN_OBJECT_CAST(obj) ((NPAinakanObject *) (obj))
#define NPAINAKAN_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NPAINAKAN_TYPE_OBJECT, NPAinakanObjectClass))
#define NPAINAKAN_IS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NPAINAKAN_TYPE_OBJECT))
#define NPAINAKAN_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NPAINAKAN_TYPE_OBJECT))
#define NPAINAKAN_OBJECT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NPAINAKAN_TYPE_OBJECT, NPAinakanObjectClass))

G_BEGIN_DECLS

typedef struct _NPAinakanObject           NPAinakanObject;
typedef struct _NPAinakanObjectClass      NPAinakanObjectClass;
typedef struct _NPAinakanObjectPrivate    NPAinakanObjectPrivate;

struct _NPAinakanObject
{
  GObject parent;

  NPAinakanObjectPrivate * priv;
};

struct _NPAinakanObjectClass
{
  GObjectClass parent_class;

  void (* destroy) (NPAinakanObject * self, GAsyncReadyCallback callback, gpointer user_data);
  void (* destroy_finish) (NPAinakanObject * self, GAsyncResult * res);
};

GType npainakan_object_get_type (void) G_GNUC_CONST;

GMainContext * npainakan_object_get_main_context (NPAinakanObject * self);

G_GNUC_INTERNAL void npainakan_object_type_init (void);
G_GNUC_INTERNAL void npainakan_object_type_deinit (void);
G_GNUC_INTERNAL gpointer npainakan_object_type_get_np_class (GType gtype);

G_END_DECLS

#endif
