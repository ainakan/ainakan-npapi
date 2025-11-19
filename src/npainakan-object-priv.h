#ifndef __NPAINAKAN_OBJECT_PRIV_H__
#define __NPAINAKAN_OBJECT_PRIV_H__

#include "npainakan-object.h"
#include "npainakan-plugin.h"

G_BEGIN_DECLS

typedef struct _NPAinakanNPObject NPAinakanNPObject;
typedef struct _NPAinakanNPObjectClass NPAinakanNPObjectClass;

struct _NPAinakanNPObject
{
  NPObject np_object;
  NPAinakanObject * g_object;
};

struct _NPAinakanNPObjectClass
{
  NPClass np_class;
  GType g_type;
  NPAinakanObjectClass * g_class;
};

G_GNUC_INTERNAL void npainakan_np_object_destroy (NPAinakanNPObject * obj);

G_END_DECLS

#endif
