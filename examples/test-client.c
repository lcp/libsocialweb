#include <mojito-client/mojito-client.h>

static void
client_view_item_added_cb (MojitoClientView *view,
                           MojitoItem       *item,
                           gpointer          userdata)
{
  g_debug ("New item: source = %s uuid = %s",
           item->source,
           item->uuid);
}

static void
client_open_view_cb (MojitoClient *client, 
                     MojitoClientView   *view,
                     gpointer      userdata)
{
  mojito_client_view_start (view);
  g_signal_connect (view, "item-added", client_view_item_added_cb, NULL);
}

static void
client_get_sources_cb (MojitoClient *client,
                       GList        *sources,
                       gpointer      userdata)
{
  GList *l;

  for (l = sources; l; l = l->next)
  {
    g_debug ("Told about source: %s", l->data);
  }

  mojito_client_open_view (client, sources, 10, client_open_view_cb, NULL);
}

int
main (int    argc,
      char **argv)
{
  MojitoClient *client;
  GMainLoop *loop;

  g_type_init ();

  client = mojito_client_new ();
  mojito_client_get_sources (client, client_get_sources_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);
}
