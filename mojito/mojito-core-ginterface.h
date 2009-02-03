#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

/**
 * MojitoCoreIface:
 *
 * Dummy typedef representing any implementation of this interface.
 */
typedef struct _MojitoCoreIface MojitoCoreIface;

/**
 * MojitoCoreIfaceClass:
 *
 * The class of MojitoCoreIface.
 */
typedef struct _MojitoCoreIfaceClass MojitoCoreIfaceClass;

GType mojito_core_iface_get_type (void);
#define MOJITO_TYPE_CORE_IFACE \
  (mojito_core_iface_get_type ())
#define MOJITO_CORE_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MOJITO_TYPE_CORE_IFACE, MojitoCoreIface))
#define MOJITO_IS_CORE_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOJITO_TYPE_CORE_IFACE))
#define MOJITO_CORE_IFACE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), MOJITO_TYPE_CORE_IFACE, MojitoCoreIfaceClass))


typedef void (*mojito_core_iface_get_services_impl) (MojitoCoreIface *self,
    DBusGMethodInvocation *context);
void mojito_core_iface_implement_get_services (MojitoCoreIfaceClass *klass, mojito_core_iface_get_services_impl impl);
/**
 * mojito_core_iface_return_from_get_services:
 * @context: The D-Bus method invocation context
 * @out_services: const gchar ** (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_core_iface_return_from_get_services (DBusGMethodInvocation *context,
    const gchar **out_services);
static inline void
mojito_core_iface_return_from_get_services (DBusGMethodInvocation *context,
    const gchar **out_services)
{
  dbus_g_method_return (context,
      out_services);
}

typedef void (*mojito_core_iface_open_view_impl) (MojitoCoreIface *self,
    const gchar **in_services,
    guint in_count,
    DBusGMethodInvocation *context);
void mojito_core_iface_implement_open_view (MojitoCoreIfaceClass *klass, mojito_core_iface_open_view_impl impl);
/**
 * mojito_core_iface_return_from_open_view:
 * @context: The D-Bus method invocation context
 * @out_view: const gchar * (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_core_iface_return_from_open_view (DBusGMethodInvocation *context,
    const gchar *out_view);
static inline void
mojito_core_iface_return_from_open_view (DBusGMethodInvocation *context,
    const gchar *out_view)
{
  dbus_g_method_return (context,
      out_view);
}



G_END_DECLS
