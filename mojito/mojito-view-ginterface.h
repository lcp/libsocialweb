#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

/**
 * MojitoViewIface:
 *
 * Dummy typedef representing any implementation of this interface.
 */
typedef struct _MojitoViewIface MojitoViewIface;

/**
 * MojitoViewIfaceClass:
 *
 * The class of MojitoViewIface.
 */
typedef struct _MojitoViewIfaceClass MojitoViewIfaceClass;

GType mojito_view_iface_get_type (void);
#define MOJITO_TYPE_VIEW_IFACE \
  (mojito_view_iface_get_type ())
#define MOJITO_VIEW_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MOJITO_TYPE_VIEW_IFACE, MojitoViewIface))
#define MOJITO_IS_VIEW_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOJITO_TYPE_VIEW_IFACE))
#define MOJITO_VIEW_IFACE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), MOJITO_TYPE_VIEW_IFACE, MojitoViewIfaceClass))


typedef void (*mojito_view_iface_start_impl) (MojitoViewIface *self,
    DBusGMethodInvocation *context);
void mojito_view_iface_implement_start (MojitoViewIfaceClass *klass, mojito_view_iface_start_impl impl);
/**
 * mojito_view_iface_return_from_start:
 * @context: The D-Bus method invocation context
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_view_iface_return_from_start (DBusGMethodInvocation *context);
static inline void
mojito_view_iface_return_from_start (DBusGMethodInvocation *context)
{
  dbus_g_method_return (context);
}

typedef void (*mojito_view_iface_refresh_impl) (MojitoViewIface *self,
    DBusGMethodInvocation *context);
void mojito_view_iface_implement_refresh (MojitoViewIfaceClass *klass, mojito_view_iface_refresh_impl impl);
/**
 * mojito_view_iface_return_from_refresh:
 * @context: The D-Bus method invocation context
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_view_iface_return_from_refresh (DBusGMethodInvocation *context);
static inline void
mojito_view_iface_return_from_refresh (DBusGMethodInvocation *context)
{
  dbus_g_method_return (context);
}

typedef void (*mojito_view_iface_stop_impl) (MojitoViewIface *self,
    DBusGMethodInvocation *context);
void mojito_view_iface_implement_stop (MojitoViewIfaceClass *klass, mojito_view_iface_stop_impl impl);
/**
 * mojito_view_iface_return_from_stop:
 * @context: The D-Bus method invocation context
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_view_iface_return_from_stop (DBusGMethodInvocation *context);
static inline void
mojito_view_iface_return_from_stop (DBusGMethodInvocation *context)
{
  dbus_g_method_return (context);
}

typedef void (*mojito_view_iface_close_impl) (MojitoViewIface *self,
    DBusGMethodInvocation *context);
void mojito_view_iface_implement_close (MojitoViewIfaceClass *klass, mojito_view_iface_close_impl impl);
/**
 * mojito_view_iface_return_from_close:
 * @context: The D-Bus method invocation context
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_view_iface_return_from_close (DBusGMethodInvocation *context);
static inline void
mojito_view_iface_return_from_close (DBusGMethodInvocation *context)
{
  dbus_g_method_return (context);
}

void mojito_view_iface_emit_item_added (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid,
    gint64 arg_date,
    GHashTable *arg_item);
void mojito_view_iface_emit_item_removed (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid);
void mojito_view_iface_emit_item_changed (gpointer instance,
    const gchar *arg_service,
    const gchar *arg_uuid,
    gint64 arg_date,
    GHashTable *arg_item);


G_END_DECLS
