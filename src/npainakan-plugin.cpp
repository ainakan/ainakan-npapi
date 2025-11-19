#include "npainakan-plugin.h"

#include "npainakan.h"
#include "npainakan-object.h"
#include "npainakan-object-priv.h"

#include <ainakan-core.h>
#ifdef G_OS_WIN32
# define VC_EXTRALEAN
# include <windows.h>
# include <tchar.h>
#endif
#include "npfunctions.h"

static gchar npainakan_mime_description[] = "application/x-vnd-ainakan:.ainakan:ole.andre.ravnas@tillitech.com";

static gint npainakan_get_process_id (void);

NPNetscapeFuncs * npainakan_nsfuncs = NULL;
GMainContext * npainakan_main_context = NULL;

G_LOCK_DEFINE_STATIC (npainakan_plugin);
static GHashTable * npainakan_plugin_roots = NULL;
static NPP npainakan_logging_instance = NULL;

static void
npainakan_log (const gchar * log_domain, GLogLevelFlags log_level, const gchar * message, gpointer user_data)
{
  NPNetscapeFuncs * browser = npainakan_nsfuncs;
  NPP instance = static_cast<NPP> (user_data);
  NPObject * window = NULL, * console = NULL;
  NPVariant variant, result;
  NPError error;

  (void) log_domain;
  (void) log_level;

  error = browser->getvalue (instance, NPNVWindowNPObject, &window);
  if (error != NPERR_NO_ERROR)
    goto beach;

  VOID_TO_NPVARIANT (variant);
  if (!browser->getproperty (instance, window, browser->getstringidentifier ("console"), &variant))
    goto beach;
  console = NPVARIANT_TO_OBJECT (variant);

  STRINGZ_TO_NPVARIANT (message, variant);
  VOID_TO_NPVARIANT (result);
  if (!browser->invoke (instance, console, browser->getstringidentifier ("log"), &variant, 1, &result))
    goto beach;
  browser->releasevariantvalue (&result);

beach:
  if (console != NULL)
    browser->releaseobject (console);
  if (window != NULL)
    browser->releaseobject (window);
}

static void
npainakan_startup (void)
{
  g_setenv ("G_DEBUG", "fatal-warnings:fatal-criticals", TRUE);
  ainakan_init ();
}

static void
npainakan_init_logging (NPP instance)
{
  if (npainakan_logging_instance != NULL)
    return;

  npainakan_logging_instance = instance;
  g_log_set_default_handler (npainakan_log, instance);
}

static void
npainakan_deinit_logging (NPP instance)
{
  if (instance != npainakan_logging_instance)
    return;

  npainakan_logging_instance = NULL;

  if (g_hash_table_size (npainakan_plugin_roots) == 0)
  {
    g_log_set_default_handler (g_log_default_handler, NULL);
  }
  else
  {
    GHashTableIter iter;
    NPP replacement_instance;

    g_hash_table_iter_init (&iter, npainakan_plugin_roots);
    g_hash_table_iter_next (&iter, reinterpret_cast<gpointer *> (&replacement_instance), NULL);
    npainakan_init_logging (replacement_instance);
  }
}

static NPError
npainakan_plugin_new (NPMIMEType plugin_type, NPP instance, uint16_t mode, int16_t argc, char * argn[], char * argv[],
    NPSavedData * saved)
{
  (void) plugin_type;
  (void) mode;
  (void) argc;
  (void) argn;
  (void) argv;
  (void) saved;

#ifdef HAVE_MAC
  NPBool supports_core_graphics;
  if (npainakan_nsfuncs->getvalue (instance, NPNVsupportsCoreGraphicsBool,
      &supports_core_graphics) == NPERR_NO_ERROR && supports_core_graphics)
  {
    npainakan_nsfuncs->setvalue (instance, NPPVpluginDrawingModel,
        reinterpret_cast<void *> (NPDrawingModelCoreGraphics));
  }

  NPBool supports_cocoa_events;
  if (npainakan_nsfuncs->getvalue (instance, NPNVsupportsCocoaBool,
      &supports_cocoa_events) == NPERR_NO_ERROR && supports_cocoa_events)
  {
    npainakan_nsfuncs->setvalue (instance, NPPVpluginEventModel,
        reinterpret_cast<void *> (NPEventModelCocoa));
  }
#endif

  npainakan_nsfuncs->setvalue (instance, NPPVpluginWindowBool, NULL);

  G_LOCK (npainakan_plugin);
  g_hash_table_insert (npainakan_plugin_roots, instance, NULL);
  npainakan_init_logging (instance);
  G_UNLOCK (npainakan_plugin);

  g_debug ("Ainakan plugin %p instantiated in pid %d", instance, npainakan_get_process_id ());

  return NPERR_NO_ERROR;
}

static NPError
npainakan_plugin_destroy (NPP instance, NPSavedData ** saved)
{
  NPAinakanNPObject * root_object;

  (void) saved;

  G_LOCK (npainakan_plugin);
  root_object = static_cast<NPAinakanNPObject *> (g_hash_table_lookup (npainakan_plugin_roots, instance));
  G_UNLOCK (npainakan_plugin);

  if (root_object != NULL)
    npainakan_np_object_destroy (root_object);

  G_LOCK (npainakan_plugin);
  g_hash_table_remove (npainakan_plugin_roots, instance);
  npainakan_deinit_logging (instance);
  G_UNLOCK (npainakan_plugin);

  g_debug ("Ainakan plugin %p destroyed in pid %d", instance, npainakan_get_process_id ());

  return NPERR_NO_ERROR;
}

static NPError
npainakan_plugin_set_window (NPP instance, NPWindow * window)
{
  (void) instance;
  (void) window;

  return NPERR_NO_ERROR;
}

static int16_t
npainakan_plugin_handle_event (NPP instance, void * ev)
{
  (void) instance;
  (void) ev;

  return NPERR_NO_ERROR;
}

static NPError
npainakan_plugin_get_value (NPP instance, NPPVariable variable, void * value)
{
  (void) instance;

  if (NP_GetValue (NULL, variable, value) == NPERR_NO_ERROR)
    return NPERR_NO_ERROR;

  switch (variable)
  {
    case NPPVpluginScriptableNPObject:
    {
      NPObject * obj;

      G_LOCK (npainakan_plugin);
      obj = static_cast<NPObject *> (g_hash_table_lookup (npainakan_plugin_roots, instance));
      if (obj == NULL)
      {
        obj = npainakan_nsfuncs->createobject (instance, static_cast<NPClass *> (npainakan_object_type_get_np_class (NPAINAKAN_TYPE_ROOT)));
        g_hash_table_insert (npainakan_plugin_roots, instance, obj);
      }
      npainakan_nsfuncs->retainobject (obj);
      G_UNLOCK (npainakan_plugin);

      *(static_cast<NPObject **> (value)) = obj;
      break;
    }
    default:
      return NPERR_INVALID_PARAM;
  }

  return NPERR_NO_ERROR;
}

static void
npainakan_root_object_destroy (gpointer data)
{
  NPObject * obj = static_cast<NPObject *> (data);
  if (obj != NULL)
    npainakan_nsfuncs->releaseobject (obj);
}

char *
NP_GetMIMEDescription (void)
{
  return npainakan_mime_description;
}

NPError OSCALL
NP_GetValue (void * reserved, NPPVariable variable, void * value)
{
  (void) reserved;

  if (value == NULL)
    return NPERR_INVALID_PARAM;

  switch (variable)
  {
    case NPPVpluginNameString:
      *(static_cast<const char **> (value)) = "Ainakan";
      break;
    case NPPVpluginDescriptionString:
      *(static_cast<const char **> (value)) = "<a href=\"http://github.com/ainakan/\">Ainakan</a> plugin.";
      break;
    default:
      return NPERR_INVALID_PARAM;
  }

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_GetEntryPoints (NPPluginFuncs * pf)
{
  npainakan_startup ();

  pf->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  pf->newp = npainakan_plugin_new;
  pf->destroy = npainakan_plugin_destroy;
  pf->setwindow = npainakan_plugin_set_window;
  pf->event = npainakan_plugin_handle_event;
  pf->getvalue = npainakan_plugin_get_value;

  return NPERR_NO_ERROR;
}

NPError OSCALL
#ifdef HAVE_LINUX
NP_Initialize (NPNetscapeFuncs * nf, NPPluginFuncs * pf)
#else
NP_Initialize (NPNetscapeFuncs * nf)
#endif
{
  if (nf == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

#ifdef HAVE_LINUX
  NP_GetEntryPoints (pf);
#else
  npainakan_startup ();
#endif

  npainakan_object_type_init ();

  npainakan_nsfuncs = nf;
  npainakan_plugin_roots = g_hash_table_new_full (NULL, NULL, NULL, npainakan_root_object_destroy);

  npainakan_main_context = ainakan_get_main_context ();

  return NPERR_NO_ERROR;
}

NPError OSCALL
NP_Shutdown (void)
{
  ainakan_shutdown ();

  npainakan_main_context = NULL;

  g_hash_table_unref (npainakan_plugin_roots);
  npainakan_plugin_roots = NULL;
  npainakan_nsfuncs = NULL;

  npainakan_object_type_deinit ();

  ainakan_deinit ();

  return NPERR_NO_ERROR;
}

void
npainakan_init_npvariant_with_string (NPVariant * var, const gchar * str)
{
  gsize len = strlen (str);
  NPUTF8 * str_copy = static_cast<NPUTF8 *> (npainakan_nsfuncs->memalloc (len));
  memcpy (str_copy, str, len);
  STRINGN_TO_NPVARIANT (str_copy, len, *var);
}

gchar *
npainakan_npstring_to_cstring (const NPString * s)
{
  gchar * str;

  str = static_cast<gchar *> (g_malloc (s->UTF8Length + 1));
  memcpy (str, s->UTF8Characters, s->UTF8Length);
  str[s->UTF8Length] = '\0';

  return str;
}

void
npainakan_init_npvariant_with_other (NPVariant * var, const NPVariant * other)
{
  memcpy (var, other, sizeof (NPVariant));

  if (other->type == NPVariantType_String)
  {
    const NPString * from = &NPVARIANT_TO_STRING (*other);
    NPString * to = &NPVARIANT_TO_STRING (*var);
    to->UTF8Characters = static_cast<NPUTF8 *> (npainakan_nsfuncs->memalloc (from->UTF8Length));
    memcpy (const_cast<NPUTF8 *> (to->UTF8Characters), from->UTF8Characters, from->UTF8Length);
  }
  else if (other->type == NPVariantType_Object)
  {
    npainakan_nsfuncs->retainobject (NPVARIANT_TO_OBJECT (*var));
  }
}

void
npainakan_npobject_release (gpointer npobject)
{
  npainakan_nsfuncs->releaseobject (static_cast<NPObject *> (npobject));
}

static gint
npainakan_get_process_id (void)
{
#ifdef G_OS_WIN32
  return GetCurrentProcessId ();
#else
  return getpid ();
#endif
}
