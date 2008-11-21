#include "mojito-core-ginterface.h"

#include "marshals.h"

static const DBusGObjectInfo _mojito_core_iface_object_info;

struct _MojitoCoreIfaceClass {
    GTypeInterface parent_class;
    mojito_core_iface_get_sources_impl get_sources;
    mojito_core_iface_open_view_impl open_view;
};

static void mojito_core_iface_base_init (gpointer klass);

GType
mojito_core_iface_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (MojitoCoreIfaceClass),
        mojito_core_iface_base_init, /* base_init */
        NULL, /* base_finalize */
        NULL, /* class_init */
        NULL, /* class_finalize */
        NULL, /* class_data */
        0,
        0, /* n_preallocs */
        NULL /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "MojitoCoreIface", &info, 0);
    }

  return type;
}

/**
 * mojito_core_iface_get_sources_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * getSources on interface com.intel.Mojito.
 */
static void
mojito_core_iface_get_sources (MojitoCoreIface *self,
    DBusGMethodInvocation *context)
{
  mojito_core_iface_get_sources_impl impl = (MOJITO_CORE_IFACE_GET_CLASS (self)->get_sources);

  if (impl != NULL)
    {
      (impl) (self,
        context);
    }
  else
    {
      GError e = { DBUS_GERROR, 
           DBUS_GERROR_UNKNOWN_METHOD,
           "Method not implemented" };

      dbus_g_method_return_error (context, &e);
    }
}

/**
 * mojito_core_iface_implement_get_sources:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the getSources D-Bus method
 *
 * Register an implementation for the getSources method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_core_iface_implement_get_sources (MojitoCoreIfaceClass *klass, mojito_core_iface_get_sources_impl impl)
{
  klass->get_sources = impl;
}

/**
 * mojito_core_iface_open_view_impl:
 * @self: The object implementing this interface
 * @in_sources: const gchar ** (FIXME, generate documentation)
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * openView on interface com.intel.Mojito.
 */
static void
mojito_core_iface_open_view (MojitoCoreIface *self,
    const gchar **in_sources,
    DBusGMethodInvocation *context)
{
  mojito_core_iface_open_view_impl impl = (MOJITO_CORE_IFACE_GET_CLASS (self)->open_view);

  if (impl != NULL)
    {
      (impl) (self,
        in_sources,
        context);
    }
  else
    {
      GError e = { DBUS_GERROR, 
           DBUS_GERROR_UNKNOWN_METHOD,
           "Method not implemented" };

      dbus_g_method_return_error (context, &e);
    }
}

/**
 * mojito_core_iface_implement_open_view:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the openView D-Bus method
 *
 * Register an implementation for the openView method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_core_iface_implement_open_view (MojitoCoreIfaceClass *klass, mojito_core_iface_open_view_impl impl)
{
  klass->open_view = impl;
}

static inline void
mojito_core_iface_base_init_once (gpointer klass G_GNUC_UNUSED)
{
  dbus_g_object_type_install_info (mojito_core_iface_get_type (),
      &_mojito_core_iface_object_info);
}
static void
mojito_core_iface_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      mojito_core_iface_base_init_once (klass);
    }
}
static const DBusGMethodInfo _mojito_core_iface_methods[] = {
  { (GCallback) mojito_core_iface_get_sources, g_cclosure_marshal_VOID__POINTER, 0 },
  { (GCallback) mojito_core_iface_open_view, mojito_marshal_VOID__BOXED_POINTER, 48 },
};

static const DBusGObjectInfo _mojito_core_iface_object_info = {
  0,
  _mojito_core_iface_methods,
  2,
"com.intel.Mojito\0getSources\0A\0sources\0O\0F\0N\0as\0\0com.intel.Mojito\0openView\0A\0sources\0I\0as\0view\0O\0F\0N\0o\0\0\0",
"\0\0",
"\0\0",
};


