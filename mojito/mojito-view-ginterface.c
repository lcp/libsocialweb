#include "mojito-view-ginterface.h"

#include "mojito-marshals.h"

static const DBusGObjectInfo _mojito_view_iface_object_info;

struct _MojitoViewIfaceClass {
    GTypeInterface parent_class;
    mojito_view_iface_start_impl start;
    mojito_view_iface_stop_impl stop;
    mojito_view_iface_close_impl close;
};

enum {
    SIGNAL_VIEW_IFACE_ItemAdded,
    SIGNAL_VIEW_IFACE_ItemRemoved,
    SIGNAL_VIEW_IFACE_ItemChanged,
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
 * Start on interface com.intel.Mojito.View.
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
 * @impl: A callback used to implement the Start D-Bus method
 *
 * Register an implementation for the Start method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_view_iface_implement_start (MojitoViewIfaceClass *klass, mojito_view_iface_start_impl impl)
{
  klass->start = impl;
}

/**
 * mojito_view_iface_stop_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * Stop on interface com.intel.Mojito.View.
 */
static void
mojito_view_iface_stop (MojitoViewIface *self,
    DBusGMethodInvocation *context)
{
  mojito_view_iface_stop_impl impl = (MOJITO_VIEW_IFACE_GET_CLASS (self)->stop);

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
 * mojito_view_iface_implement_stop:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the Stop D-Bus method
 *
 * Register an implementation for the Stop method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_view_iface_implement_stop (MojitoViewIfaceClass *klass, mojito_view_iface_stop_impl impl)
{
  klass->stop = impl;
}

/**
 * mojito_view_iface_close_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * Close on interface com.intel.Mojito.View.
 */
static void
mojito_view_iface_close (MojitoViewIface *self,
    DBusGMethodInvocation *context)
{
  mojito_view_iface_close_impl impl = (MOJITO_VIEW_IFACE_GET_CLASS (self)->close);

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
 * mojito_view_iface_implement_close:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the Close D-Bus method
 *
 * Register an implementation for the Close method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_view_iface_implement_close (MojitoViewIfaceClass *klass, mojito_view_iface_close_impl impl)
{
  klass->close = impl;
}

/**
 * mojito_view_iface_emit_item_added:
 * @instance: The object implementing this interface
 * @arg_service: const gchar * (FIXME, generate documentation)
 * @arg_uuid: const gchar * (FIXME, generate documentation)
 * @arg_date: gint64  (FIXME, generate documentation)
 * @arg_item: GHashTable * (FIXME, generate documentation)
 *
 * Type-safe wrapper around g_signal_emit to emit the
 * ItemAdded signal on interface com.intel.Mojito.View.
 */
void
mojito_view_iface_emit_item_added (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid,
    gint64 arg_date,
    GHashTable *arg_item)
{
  g_assert (instance != NULL);
  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (instance, MOJITO_TYPE_VIEW_IFACE));
  g_signal_emit (instance,
      view_iface_signals[SIGNAL_VIEW_IFACE_ItemAdded],
      0,
      arg_service,
      arg_uuid,
      arg_date,
      arg_item);
}

/**
 * mojito_view_iface_emit_item_removed:
 * @instance: The object implementing this interface
 * @arg_service: const gchar * (FIXME, generate documentation)
 * @arg_uuid: const gchar * (FIXME, generate documentation)
 *
 * Type-safe wrapper around g_signal_emit to emit the
 * ItemRemoved signal on interface com.intel.Mojito.View.
 */
void
mojito_view_iface_emit_item_removed (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid)
{
  g_assert (instance != NULL);
  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (instance, MOJITO_TYPE_VIEW_IFACE));
  g_signal_emit (instance,
      view_iface_signals[SIGNAL_VIEW_IFACE_ItemRemoved],
      0,
      arg_service,
      arg_uuid);
}

/**
 * mojito_view_iface_emit_item_changed:
 * @instance: The object implementing this interface
 * @arg_service: const gchar * (FIXME, generate documentation)
 * @arg_uuid: const gchar * (FIXME, generate documentation)
 * @arg_date: gint64  (FIXME, generate documentation)
 * @arg_item: GHashTable * (FIXME, generate documentation)
 *
 * Type-safe wrapper around g_signal_emit to emit the
 * ItemChanged signal on interface com.intel.Mojito.View.
 */
void
mojito_view_iface_emit_item_changed (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid,
    gint64 arg_date,
    GHashTable *arg_item)
{
  g_assert (instance != NULL);
  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (instance, MOJITO_TYPE_VIEW_IFACE));
  g_signal_emit (instance,
      view_iface_signals[SIGNAL_VIEW_IFACE_ItemChanged],
      0,
      arg_service,
      arg_uuid,
      arg_date,
      arg_item);
}

static inline void
mojito_view_iface_base_init_once (gpointer klass G_GNUC_UNUSED)
{
  /**
   * MojitoViewIface::item-added:
   * @arg_service: const gchar * (FIXME, generate documentation)
   * @arg_uuid: const gchar * (FIXME, generate documentation)
   * @arg_date: gint64  (FIXME, generate documentation)
   * @arg_item: GHashTable * (FIXME, generate documentation)
   *
   * The ItemAdded D-Bus signal is emitted whenever this GObject signal is.
   */
  view_iface_signals[SIGNAL_VIEW_IFACE_ItemAdded] =
  g_signal_new ("item-added",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      mojito_marshal_VOID__STRING_STRING_INT64_BOXED,
      G_TYPE_NONE,
      4,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64,
      DBUS_TYPE_G_STRING_STRING_HASHTABLE);

  /**
   * MojitoViewIface::item-removed:
   * @arg_service: const gchar * (FIXME, generate documentation)
   * @arg_uuid: const gchar * (FIXME, generate documentation)
   *
   * The ItemRemoved D-Bus signal is emitted whenever this GObject signal is.
   */
  view_iface_signals[SIGNAL_VIEW_IFACE_ItemRemoved] =
  g_signal_new ("item-removed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      mojito_marshal_VOID__STRING_STRING,
      G_TYPE_NONE,
      2,
      G_TYPE_STRING,
      G_TYPE_STRING);

  /**
   * MojitoViewIface::item-changed:
   * @arg_service: const gchar * (FIXME, generate documentation)
   * @arg_uuid: const gchar * (FIXME, generate documentation)
   * @arg_date: gint64  (FIXME, generate documentation)
   * @arg_item: GHashTable * (FIXME, generate documentation)
   *
   * The ItemChanged D-Bus signal is emitted whenever this GObject signal is.
   */
  view_iface_signals[SIGNAL_VIEW_IFACE_ItemChanged] =
  g_signal_new ("item-changed",
      G_OBJECT_CLASS_TYPE (klass),
      G_SIGNAL_RUN_LAST|G_SIGNAL_DETAILED,
      0,
      NULL, NULL,
      mojito_marshal_VOID__STRING_STRING_INT64_BOXED,
      G_TYPE_NONE,
      4,
      G_TYPE_STRING,
      G_TYPE_STRING,
      G_TYPE_INT64,
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
  { (GCallback) mojito_view_iface_stop, g_cclosure_marshal_VOID__POINTER, 31 },
  { (GCallback) mojito_view_iface_close, g_cclosure_marshal_VOID__POINTER, 61 },
};

static const DBusGObjectInfo _mojito_view_iface_object_info = {
  0,
  _mojito_view_iface_methods,
  3,
"com.intel.Mojito.View\0Start\0A\0\0com.intel.Mojito.View\0Stop\0A\0\0com.intel.Mojito.View\0Close\0A\0\0\0",
"com.intel.Mojito.View\0ItemAdded\0com.intel.Mojito.View\0ItemRemoved\0com.intel.Mojito.View\0ItemChanged\0\0",
"\0\0",
};


