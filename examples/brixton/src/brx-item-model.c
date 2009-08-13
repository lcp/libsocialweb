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

#include <mojito-client/mojito-client-view.h>

#include "brx-item-model.h"

G_DEFINE_TYPE (BrxItemModel, brx_item_model, CLUTTER_TYPE_LIST_MODEL)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRX_TYPE_ITEM_MODEL, BrxItemModelPrivate))

typedef struct _BrxItemModelPrivate BrxItemModelPrivate;

struct _BrxItemModelPrivate {
  MojitoClientView *view;
};

enum
{
  PROP_0,
  PROP_VIEW
};

static void brx_item_model_set_view (BrxItemModel     *model,
                                     MojitoClientView *view);


static void
brx_item_model_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  BrxItemModelPrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_VIEW:
      g_value_set_object (value, priv->view);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
brx_item_model_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
    case PROP_VIEW:
      brx_item_model_set_view ((BrxItemModel *)object,
                               (MojitoClientView *)g_value_get_object (value));
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
brx_item_model_dispose (GObject *object)
{
  BrxItemModelPrivate *priv = GET_PRIVATE (object);

  /* Dispose can be called multiple times, must check for re-entrancy */
  if (priv->view)
  {
    g_object_unref (priv->view);
    priv->view = NULL;
  }

  G_OBJECT_CLASS (brx_item_model_parent_class)->dispose (object);
}

static void
brx_item_model_finalize (GObject *object)
{
  G_OBJECT_CLASS (brx_item_model_parent_class)->finalize (object);
}

static void
brx_item_model_class_init (BrxItemModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (BrxItemModelPrivate));

  object_class->get_property = brx_item_model_get_property;
  object_class->set_property = brx_item_model_set_property;
  object_class->dispose = brx_item_model_dispose;
  object_class->finalize = brx_item_model_finalize;

  /* Register the propert to hold the view */
  pspec = g_param_spec_object ("view",
                               "View",
                               "View that this model will represent",
                               MOJITO_TYPE_CLIENT_VIEW,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  g_object_class_install_property (object_class, PROP_VIEW, pspec);
}

static void
brx_item_model_init (BrxItemModel *self)
{
  GType types[] = { MOJITO_TYPE_ITEM };

  /* Set the types (or type in this case) that the model will hold */
  clutter_model_set_types (CLUTTER_MODEL (self),
                           1,
                           types);
}

ClutterModel *
brx_item_model_new (MojitoClientView *view)
{
  return g_object_new (BRX_TYPE_ITEM_MODEL,
                       "view", view,
                       NULL);
}

static void
_view_item_added_cb (MojitoClientView *view,
                     MojitoItem       *item,
                     gpointer          userdata)
{
  clutter_model_prepend (CLUTTER_MODEL (userdata),
                         0, item,
                         -1);
}

static void
_view_item_removed_cb (MojitoClientView *view,
                       MojitoItem       *item_in,
                       gpointer          userdata)
{
  ClutterModelIter *iter;
  MojitoItem *item;

  /* To find the item to remove from the model we must do an O(n) search
   * comparing the uuids.
   */
  iter = clutter_model_get_first_iter (CLUTTER_MODEL (userdata));

  while (!clutter_model_iter_is_last (iter))
  {
    clutter_model_iter_get (iter,
                            0, &item,
                            -1);

    if (g_str_equal (item->uuid, item_in->uuid))
    {
      clutter_model_remove (CLUTTER_MODEL (userdata),
                            clutter_model_iter_get_row (iter));
      break;
    }

    clutter_model_iter_next (iter);
  }

  g_object_unref (iter);
}

static void
brx_item_model_set_view (BrxItemModel     *model,
                         MojitoClientView *view)
{
  BrxItemModelPrivate *priv = GET_PRIVATE (model);

  priv->view = g_object_ref (view);

  /* Connect to signals from the view and update the model */
  g_signal_connect (priv->view,
                    "item-added",
                    (GCallback)_view_item_added_cb,
                    model);
  g_signal_connect (priv->view,
                    "item-removed",
                    (GCallback)_view_item_removed_cb,
                    model);
}


