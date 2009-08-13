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

#include <mojito-client/mojito-item.h>
#include "brx-item.h"

G_DEFINE_TYPE (BrxItem, brx_item, NBTK_TYPE_BUTTON)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRX_TYPE_ITEM, BrxItemPrivate))

typedef struct _BrxItemPrivate BrxItemPrivate;

struct _BrxItemPrivate {
  MojitoItem *item;
  ClutterActor *tex;
};

enum
{
  PROP_0,
  PROP_ITEM
};

static void brx_item_update (BrxItem *item);

static void
brx_item_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  BrxItemPrivate *priv =  GET_PRIVATE (object);

  switch (property_id) {
    case PROP_ITEM:
      g_value_set_boxed (value, priv->item);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
brx_item_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  BrxItemPrivate *priv =  GET_PRIVATE (object);
  MojitoItem *item;

  switch (property_id) {
    case PROP_ITEM:
      item = g_value_get_boxed (value);

      /* Abort early if they are the same */
      if (item == priv->item)
        break;

      if (priv->item)
        mojito_item_unref (priv->item);

      if (item)
        priv->item = mojito_item_ref (item);
      else
        priv->item = NULL;

      if (priv->item)
        brx_item_update ((BrxItem *)object);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
brx_item_dispose (GObject *object)
{
  BrxItemPrivate *priv = GET_PRIVATE (object);

  /* Dispose can be called multiple times, must check for re-entrancy */
  if (priv->item)
  {
    mojito_item_unref (priv->item);
    priv->item = NULL;
  }

  G_OBJECT_CLASS (brx_item_parent_class)->dispose (object);
}

static void
brx_item_finalize (GObject *object)
{
  G_OBJECT_CLASS (brx_item_parent_class)->finalize (object);
}

static void
brx_item_class_init (BrxItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (BrxItemPrivate));

  object_class->get_property = brx_item_get_property;
  object_class->set_property = brx_item_set_property;
  object_class->dispose = brx_item_dispose;
  object_class->finalize = brx_item_finalize;

  /* Create 'item' property and add it to the GObject */
  pspec = g_param_spec_boxed ("item",
                              "Item",
                              "The item to render",
                              MOJITO_TYPE_ITEM,
                              G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_ITEM, pspec);
}

static void
brx_item_init (BrxItem *self)
{
  BrxItemPrivate *priv = GET_PRIVATE (self);

  /* To contain our thumbnail */
  priv->tex = clutter_texture_new ();

  /*
   * We are an NbtkButton subclass which descends from NbtkBin which is a
   * single child container.
   */
  nbtk_bin_set_child (NBTK_BIN (self), priv->tex);

  /* Set a static size on the thumbnail */
  clutter_actor_set_size (priv->tex, 128, 128);
}

/* Update the texture and the tooltip based on the item */
static void
brx_item_update (BrxItem *item)
{
  BrxItemPrivate *priv = GET_PRIVATE (item);
  const gchar *thumbnail;
  const gchar *title;
  GError *error = NULL;

  /* Get thumbnail and title values from the MojitoItem */
  thumbnail = g_hash_table_lookup (priv->item->props,
                                   "thumbnail");
  title = g_hash_table_lookup (priv->item->props,
                               "title");

  /* Load a file into the texture */
  if (!clutter_texture_set_from_file (CLUTTER_TEXTURE (priv->tex),
                                      thumbnail,
                                      &error))
  {
    g_critical (G_STRLOC ": Error opening thumbnail: %s",
                error->message);
    g_clear_error (&error);
  }

  /* Set a tooltip containing the title */
  nbtk_widget_set_tooltip_text (NBTK_WIDGET (item),
                                title);
}

