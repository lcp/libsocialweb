#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "mojito-client.h"

#include "mojito-core-bindings.h"

G_DEFINE_TYPE (MojitoClient, mojito_client, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_CLIENT, MojitoClientPrivate))

typedef struct _MojitoClientPrivate MojitoClientPrivate;

struct _MojitoClientPrivate {
  DBusGConnection *connection;
  DBusGProxy *proxy;
};

#define MOJITO_SERVICE_NAME "com.intel.Mojito"
#define MOJITO_SERVICE_CORE_OBJECT "/com/intel/Mojito"
#define MOJITO_SERVICE_CORE_INTERFACE "com.intel.Mojito"

static void
mojito_client_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_client_dispose (GObject *object)
{
  G_OBJECT_CLASS (mojito_client_parent_class)->dispose (object);
}

static void
mojito_client_finalize (GObject *object)
{
  G_OBJECT_CLASS (mojito_client_parent_class)->finalize (object);
}

static void
mojito_client_class_init (MojitoClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoClientPrivate));

  object_class->get_property = mojito_client_get_property;
  object_class->set_property = mojito_client_set_property;
  object_class->dispose = mojito_client_dispose;
  object_class->finalize = mojito_client_finalize;
}


static void
mojito_client_init (MojitoClient *self)
{
  MojitoClientPrivate *priv = GET_PRIVATE (self);
  GError *error = NULL;
  DBusConnection *conn;
  DBusError derror;

  priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (!priv->connection)
  {
    g_critical (G_STRLOC ": Error getting DBUS connection: %s",
                error->message);
    g_clear_error (&error);
    return;
  }

  conn = dbus_g_connection_get_connection (priv->connection);
  dbus_error_init (&derror);
  if (!dbus_bus_start_service_by_name (conn,
                                       MOJITO_SERVICE_NAME,
                                       0, 
                                       NULL, 
                                       &derror))
  {
    g_critical (G_STRLOC ": Error starting mojito service: %s",
                derror.message);
    dbus_error_free (&derror);
    return;
  }

  priv->proxy = dbus_g_proxy_new_for_name_owner (priv->connection,
                                                 MOJITO_SERVICE_NAME,
                                                 MOJITO_SERVICE_CORE_OBJECT,
                                                 MOJITO_SERVICE_CORE_INTERFACE,
                                                 &error);

  if (!priv->proxy)
  {
    g_critical (G_STRLOC ": Error setting up proxy for remote object: %s",
                error->message);
    g_clear_error (&error);
  }
}

MojitoClient *
mojito_client_new (void)
{
  return g_object_new (MOJITO_TYPE_CLIENT, NULL);
}

typedef struct
{
  MojitoClient *client;
  MojitoClientOpenViewCallback cb;
  gpointer userdata;
} OpenViewClosure;

static void
_mojito_client_open_view_cb (DBusGProxy *proxy,
                             gchar      *view_path,
                             GError     *error,
                             gpointer    userdata)
{
  OpenViewClosure *closure = (OpenViewClosure *)userdata;
  MojitoClient *client = closure->client;
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  MojitoClientView *view;

  view = _mojito_client_view_new_for_path (view_path);

  closure->cb (client, view, closure->userdata);

  g_free (view_path);

  g_object_unref (closure->client);
  g_free (closure);
}

void
mojito_client_open_view (MojitoClient                *client,
                         GList                       *sources,
                         guint                        count,
                         MojitoClientOpenViewCallback cb,
                         gpointer                     userdata)
{
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  GPtrArray *sources_array;
  GList *l;
  OpenViewClosure *closure;

  sources_array = g_ptr_array_new ();

  for (l = sources; l; l = l->next)
  {
    g_ptr_array_add (sources_array, l->data);
  }

  g_ptr_array_add (sources_array, NULL);

  closure = g_new0 (OpenViewClosure, 1);
  closure->client = g_object_ref (client);
  closure->cb = cb;
  closure->userdata = userdata;

  com_intel_Mojito_open_view_async (priv->proxy,
                                    (const gchar **)sources_array->pdata,
                                    count,
                                    _mojito_client_open_view_cb,
                                    closure);

  g_ptr_array_free (sources_array, TRUE);
}

typedef struct
{
  MojitoClient *client;
  MojitoClientGetSourcesCallback cb;
  gpointer userdata;
} GetSourcesClosure;

static void
_mojito_client_get_sources_cb (DBusGProxy *proxy,
                               gchar     **sources,
                               GError     *error,
                               gpointer    userdata)
{
  GetSourcesClosure *closure = (GetSourcesClosure *)userdata;
  MojitoClient *client = closure->client;

  GList *sources_list = NULL;
  gchar *source;
  gint i;

  for (i = 0; (source = sources[i]); i++)
  {
    sources_list = g_list_append (sources_list, source);
  }

  closure->cb (client, sources_list, closure->userdata);

  g_list_free (sources_list);
  g_strfreev (sources);
}

void 
mojito_client_get_sources (MojitoClient                  *client,
                           MojitoClientGetSourcesCallback cb,
                           gpointer                       userdata)
{
  MojitoClientPrivate *priv = GET_PRIVATE (client);
  GetSourcesClosure *closure;

  closure = g_new0 (GetSourcesClosure, 1);
  closure->client = g_object_ref (client);
  closure->cb = cb;
  closure->userdata = userdata;

  com_intel_Mojito_get_sources_async (priv->proxy,
                                      _mojito_client_get_sources_cb,
                                      closure);
}