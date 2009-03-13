#include "mojito-service-ginterface.h"

#include "mojito-marshals.h"

static const DBusGObjectInfo _mojito_service_iface_object_info;

struct _MojitoServiceIfaceClass {
    GTypeInterface parent_class;
    mojito_service_iface_get_persona_icon_impl get_persona_icon;
    mojito_service_iface_update_status_impl update_status;
    mojito_service_iface_get_capabilities_impl get_capabilities;
};

static void mojito_service_iface_base_init (gpointer klass);

GType
mojito_service_iface_get_type (void)
{
  static GType type = 0;

  if (G_UNLIKELY (type == 0))
    {
      static const GTypeInfo info = {
        sizeof (MojitoServiceIfaceClass),
        mojito_service_iface_base_init, /* base_init */
        NULL, /* base_finalize */
        NULL, /* class_init */
        NULL, /* class_finalize */
        NULL, /* class_data */
        0,
        0, /* n_preallocs */
        NULL /* instance_init */
      };

      type = g_type_register_static (G_TYPE_INTERFACE,
          "MojitoServiceIface", &info, 0);
    }

  return type;
}

/**
 * mojito_service_iface_get_persona_icon_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * GetPersonaIcon on interface com.intel.Mojito.Service.
 */
static void
mojito_service_iface_get_persona_icon (MojitoServiceIface *self,
    DBusGMethodInvocation *context)
{
  mojito_service_iface_get_persona_icon_impl impl = (MOJITO_SERVICE_IFACE_GET_CLASS (self)->get_persona_icon);

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
 * mojito_service_iface_implement_get_persona_icon:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the GetPersonaIcon D-Bus method
 *
 * Register an implementation for the GetPersonaIcon method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_service_iface_implement_get_persona_icon (MojitoServiceIfaceClass *klass, mojito_service_iface_get_persona_icon_impl impl)
{
  klass->get_persona_icon = impl;
}

/**
 * mojito_service_iface_update_status_impl:
 * @self: The object implementing this interface
 * @in_status_message: const gchar * (FIXME, generate documentation)
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * UpdateStatus on interface com.intel.Mojito.Service.
 */
static void
mojito_service_iface_update_status (MojitoServiceIface *self,
    const gchar *in_status_message,
    DBusGMethodInvocation *context)
{
  mojito_service_iface_update_status_impl impl = (MOJITO_SERVICE_IFACE_GET_CLASS (self)->update_status);

  if (impl != NULL)
    {
      (impl) (self,
        in_status_message,
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
 * mojito_service_iface_implement_update_status:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the UpdateStatus D-Bus method
 *
 * Register an implementation for the UpdateStatus method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_service_iface_implement_update_status (MojitoServiceIfaceClass *klass, mojito_service_iface_update_status_impl impl)
{
  klass->update_status = impl;
}

/**
 * mojito_service_iface_get_capabilities_impl:
 * @self: The object implementing this interface
 * @context: Used to return values or throw an error
 *
 * The signature of an implementation of the D-Bus method
 * GetCapabilities on interface com.intel.Mojito.Service.
 */
static void
mojito_service_iface_get_capabilities (MojitoServiceIface *self,
    DBusGMethodInvocation *context)
{
  mojito_service_iface_get_capabilities_impl impl = (MOJITO_SERVICE_IFACE_GET_CLASS (self)->get_capabilities);

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
 * mojito_service_iface_implement_get_capabilities:
 * @klass: A class whose instances implement this interface
 * @impl: A callback used to implement the GetCapabilities D-Bus method
 *
 * Register an implementation for the GetCapabilities method in the vtable
 * of an implementation of this interface. To be called from
 * the interface init function.
 */
void
mojito_service_iface_implement_get_capabilities (MojitoServiceIfaceClass *klass, mojito_service_iface_get_capabilities_impl impl)
{
  klass->get_capabilities = impl;
}

static inline void
mojito_service_iface_base_init_once (gpointer klass G_GNUC_UNUSED)
{
  dbus_g_object_type_install_info (mojito_service_iface_get_type (),
      &_mojito_service_iface_object_info);
}
static void
mojito_service_iface_base_init (gpointer klass)
{
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      mojito_service_iface_base_init_once (klass);
    }
}
static const DBusGMethodInfo _mojito_service_iface_methods[] = {
  { (GCallback) mojito_service_iface_get_persona_icon, g_cclosure_marshal_VOID__POINTER, 0 },
  { (GCallback) mojito_service_iface_update_status, mojito_marshal_VOID__STRING_POINTER, 55 },
  { (GCallback) mojito_service_iface_get_capabilities, g_cclosure_marshal_VOID__POINTER, 131 },
};

static const DBusGObjectInfo _mojito_service_iface_object_info = {
  0,
  _mojito_service_iface_methods,
  3,
"com.intel.Mojito.Service\0GetPersonaIcon\0A\0uri\0O\0F\0N\0s\0\0com.intel.Mojito.Service\0UpdateStatus\0A\0status_message\0I\0s\0success\0O\0F\0N\0b\0\0com.intel.Mojito.Service\0GetCapabilities\0A\0can_get_persona_icon\0O\0F\0N\0b\0can_update_status\0O\0F\0N\0b\0\0\0",
"\0\0",
"\0\0",
};


