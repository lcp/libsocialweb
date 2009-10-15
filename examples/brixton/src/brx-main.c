/*
 * Brixton, a Mojito demo
 * Copyright (C) 2009, Intel Corporation.
 *
 * Authors: Rob Bradford <rob@linux.intel.com>
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <mojito-client/mojito-client.h>
#include <nbtk/nbtk.h>
#include "brx-item.h"
#include "brx-item-model.h"

typedef struct {
  ClutterActor *stage;
  NbtkWidget *item_view;
  MojitoClient *client;
  ClutterModel *model;
} BrixtonApp;

static void 
_client_view_opened_cb (MojitoClient     *client,
                        MojitoClientView *view,
                        gpointer          userdata)
{
  BrixtonApp *app = (BrixtonApp *)userdata;

  /* Create the model subclass based on the view from Mojito and set this
   * model on the item view.
   */
  app->model = brx_item_model_new (view);
  nbtk_item_view_set_model (NBTK_ITEM_VIEW (app->item_view),
                            app->model);

  /* Start the view, causing data to be pulled in */
  mojito_client_view_start (view);
}

int
main (int    argc,
      char **argv)
{
  BrixtonApp *app = g_new0 (BrixtonApp, 1);
  NbtkStyle *style;
  GError *error = NULL;

  clutter_init (&argc, &argv);

  /* Load in our stylesheet */
  style = nbtk_style_get_default ();
  if (!nbtk_style_load_from_file (style,
                                  PKG_DATA_DIR "/style.css",
                                  &error))
  {
    g_critical (G_STRLOC ": Error loading stylesheet: %s",
                error->message);
    g_clear_error (&error);
  }

  /* Create our stage */
  app->stage = clutter_stage_get_default ();

  /* Create our item view to render our items, setting the item type to render
   * and also mapping the properties on the item to a particular column of the
   * model. In this case, we map "item" to column zero (the only column.)
   */
  app->item_view = nbtk_item_view_new ();
  nbtk_item_view_set_item_type (NBTK_ITEM_VIEW (app->item_view),
                                BRX_TYPE_ITEM);
  nbtk_item_view_add_attribute (NBTK_ITEM_VIEW (app->item_view),
                                "item",
                                0);

  /* Add the item view to the stage. Since the Stage is a Group it is a
   * boundless infinite plan. We must size the item view to the size of the
   * stage in order to get the correct behaviour
   */
  clutter_container_add_actor (CLUTTER_CONTAINER (app->stage),
                               (ClutterActor *)app->item_view);
  clutter_actor_set_size ((ClutterActor *)app->item_view,
                          clutter_actor_get_width (app->stage),
                          clutter_actor_get_height (app->stage));

  /* Create the client side object for Mojito and open a view for Lastfm. This
   * is an asynchronous call so a callback will be fired when it is ready
   */
  app->client = mojito_client_new ();
  mojito_client_open_view_for_service (app->client,
                                       "lastfm",
                                       10,
                                       _client_view_opened_cb,
                                       app);

  /* Show the stage and start up the main loop */
  clutter_actor_show (app->stage);
  clutter_main ();
}
