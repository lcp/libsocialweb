#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

/**
 * MojitoServiceIface:
 *
 * Dummy typedef representing any implementation of this interface.
 */
typedef struct _MojitoServiceIface MojitoServiceIface;

/**
 * MojitoServiceIfaceClass:
 *
 * The class of MojitoServiceIface.
 */
typedef struct _MojitoServiceIfaceClass MojitoServiceIfaceClass;

GType mojito_service_iface_get_type (void);
#define MOJITO_TYPE_SERVICE_IFACE \
  (mojito_service_iface_get_type ())
#define MOJITO_SERVICE_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), MOJITO_TYPE_SERVICE_IFACE, MojitoServiceIface))
#define MOJITO_IS_SERVICE_IFACE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), MOJITO_TYPE_SERVICE_IFACE))
#define MOJITO_SERVICE_IFACE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_INTERFACE((obj), MOJITO_TYPE_SERVICE_IFACE, MojitoServiceIfaceClass))


typedef void (*mojito_service_iface_get_last_item_impl) (MojitoServiceIface *self,
    DBusGMethodInvocation *context);
void mojito_service_iface_implement_get_last_item (MojitoServiceIfaceClass *klass, mojito_service_iface_get_last_item_impl impl);
/**
 * mojito_service_iface_return_from_get_last_item:
 * @context: The D-Bus method invocation context
 * @out_item: GHashTable * (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_service_iface_return_from_get_last_item (DBusGMethodInvocation *context,
    GHashTable *out_item);
static inline void
mojito_service_iface_return_from_get_last_item (DBusGMethodInvocation *context,
    GHashTable *out_item)
{
  dbus_g_method_return (context,
      out_item);
}

typedef void (*mojito_service_iface_get_persona_icon_impl) (MojitoServiceIface *self,
    DBusGMethodInvocation *context);
void mojito_service_iface_implement_get_persona_icon (MojitoServiceIfaceClass *klass, mojito_service_iface_get_persona_icon_impl impl);
/**
 * mojito_service_iface_return_from_get_persona_icon:
 * @context: The D-Bus method invocation context
 * @out_uri: const gchar * (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_service_iface_return_from_get_persona_icon (DBusGMethodInvocation *context,
    const gchar *out_uri);
static inline void
mojito_service_iface_return_from_get_persona_icon (DBusGMethodInvocation *context,
    const gchar *out_uri)
{
  dbus_g_method_return (context,
      out_uri);
}

typedef void (*mojito_service_iface_update_status_impl) (MojitoServiceIface *self,
    const gchar *in_status_message,
    DBusGMethodInvocation *context);
void mojito_service_iface_implement_update_status (MojitoServiceIfaceClass *klass, mojito_service_iface_update_status_impl impl);
/**
 * mojito_service_iface_return_from_update_status:
 * @context: The D-Bus method invocation context
 * @out_success: gboolean  (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_service_iface_return_from_update_status (DBusGMethodInvocation *context,
    gboolean out_success);
static inline void
mojito_service_iface_return_from_update_status (DBusGMethodInvocation *context,
    gboolean out_success)
{
  dbus_g_method_return (context,
      out_success);
}

typedef void (*mojito_service_iface_get_capabilities_impl) (MojitoServiceIface *self,
    DBusGMethodInvocation *context);
void mojito_service_iface_implement_get_capabilities (MojitoServiceIfaceClass *klass, mojito_service_iface_get_capabilities_impl impl);
/**
 * mojito_service_iface_return_from_get_capabilities:
 * @context: The D-Bus method invocation context
 * @out_can_get_last_status: gboolean  (FIXME, generate documentation)
 * @out_can_get_persona_icon: gboolean  (FIXME, generate documentation)
 * @out_can_update_status: gboolean  (FIXME, generate documentation)
 *
 * Return successfully by calling dbus_g_method_return().
 * This inline function exists only to provide type-safety.
 */
static inline
/* this comment is to stop gtkdoc realising this is static */
void mojito_service_iface_return_from_get_capabilities (DBusGMethodInvocation *context,
    gboolean out_can_get_last_status,
    gboolean out_can_get_persona_icon,
    gboolean out_can_update_status);
static inline void
mojito_service_iface_return_from_get_capabilities (DBusGMethodInvocation *context,
    gboolean out_can_get_last_status,
    gboolean out_can_get_persona_icon,
    gboolean out_can_update_status)
{
  dbus_g_method_return (context,
      out_can_get_last_status,
      out_can_get_persona_icon,
      out_can_update_status);
}



G_END_DECLS
