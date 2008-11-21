#include "mojito-view-ginterface.h"

#include "marshals.h"

static const DBusGObjectInfo _mojito_view_iface_object_info;

struct _MojitoViewIfaceClass {
    GTypeInterface parent_class;
    mojito_view_iface_start_impl start;
};

enum {
    SIGNAL_VIEW_IFACE_ItemAdded,
    N_VIEW_IFACE_SIGNALS
};
static guint view_iface_signals[N_VIEW_IFACE_SIGNALS] = {0};

static void mojito_view_iface_base_init (gpointer klass);

GType
mojito_view_iface_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (MojitoViewIfaceClass),
        mojito_view_iface_base_init, /* base_init */
        NULL, /* base_finalize */
        NULL, /* class_init */
        NULL, /* class_finalize */
        NULL, /* class_data */
        0,
        0, /* n_preallocs */
        NULL /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "MojitoViewIface", &info, 0);
    }

  return type;
}

/**
 * mojito_view_iface_start_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * start on interface com.intel.Mojito.View.
 */
static void
mojito_view_iface_start (MojitoViewIface *self,
    DBusGMethodInvocation *context)
{
  mojito_view_iface_start_impl impl = (MOJITO_VIEW_IFACE_GET_CLASS (self)->start);

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
 * mojito_view_iface_implement_start:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the start D-Bus method
 *
 * Register an implementation for the start method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_view_iface_implement_start (MojitoViewIfaceClass *klass, mojito_view_iface_start_impl impl)
{
  klass->start = impl;
}

/**
 * mojito_view_iface_emit_item_added:
 * @instance: The object implementing this interface
 * @arg_item: const gchar * (FIXME, generate documentation)
 * @arg_data: GHashTable * (FIXME, generate documentation)
 *
 * Type-safe wrapper around g_signal_emit to emit the
 * ItemAdded signal on interface com.intel.Mojito.View.
 */
void
mojito_view_iface_emit_item_added (gpointer instance,
    const gchar *arg_item,
    GHashTable *arg_data)
{
  g_assert (instance != NULL);
  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (instance, MOJITO_TYPE_VIEW_IFACE));
  g_signal_emit (instance,
      view_iface_signals[SIGNAL_VIEW_IFACE_ItemAdded],
      0,
      arg_item,
      arg_data);
}

static inline void
mojito_view_iface_base_init_once (gpointer klass G_GNUC_UNUSED)
{
  /**
   * MojitoViewIface::item-added:
   * @arg_item: const gchar * (FIXME, generate documentation)
   * @arg_data: GHashTable * (FIXME, generate documentation)
   *
   * The ItemAdded D-Bus signal is emitted whenever this GObject signal is.
   */
  view_iface_signals[SIGNAL_VIEW_IFACE_ItemAdded] =
  g_signal_new ("item-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      mojito_marshal_VOID__STRING_BOXED,
      G_TYPE_NONE,
      2,
      G_TYPE_STRING,
      DBUS_TYPE_G_STRING_STRING_HASHTABLE);

  dbus_g_object_type_install_info (mojito_view_iface_get_type (),
      &_mojito_view_iface_object_info);
}
static void
mojito_view_iface_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      mojito_view_iface_base_init_once (klass);
    }
}
static const DBusGMethodInfo _mojito_view_iface_methods[] = {
  { (GCallback) mojito_view_iface_start, g_cclosure_marshal_VOID__POINTER, 0 },
};

static const DBusGObjectInfo _mojito_view_iface_object_info = {
  0,
  _mojito_view_iface_methods,
  1,
"com.intel.Mojito.View\0start\0A\0\0\0",
"com.intel.Mojito.View\0ItemAdded\0\0",
"\0\0",
};


