#ifndef __NPAINAKAN_BYTE_ARRAY_H__
#define __NPAINAKAN_BYTE_ARRAY_H__

#include "npainakan-plugin.h"

G_BEGIN_DECLS

NPClass * npainakan_byte_array_get_class (void) G_GNUC_CONST;

NPObject * npainakan_byte_array_new (NPP npp, const guint8 * data, gint data_length);

G_END_DECLS

#endif
