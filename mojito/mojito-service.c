/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
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
 */

#include <glib/gstdio.h>

#include "mojito-service.h"
#include "mojito-marshals.h"
#include "mojito-core.h"
#include "mojito-item.h"

G_DEFINE_ABSTRACT_TYPE (MojitoService, mojito_service, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE, MojitoServicePrivate))

typedef struct _MojitoServicePrivate MojitoServicePrivate;

struct _MojitoServicePrivate {
  MojitoCore *core;
};

enum {
  PROP_0,
  PROP_CORE
};

static void
mojito_service_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_CORE:
      priv->core = g_value_dup_object (value);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  switch (property_id) {
    case PROP_CORE:
      g_value_set_object (value, priv->core);
      break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mojito_service_dispose (GObject *object)
{
  MojitoServicePrivate *priv = GET_PRIVATE (object);

  if (priv->core)
  {
    g_object_unref (priv->core);
    priv->core = NULL;
  }

  G_OBJECT_CLASS (mojito_service_parent_class)->dispose (object);
}

static void
mojito_service_class_init (MojitoServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *pspec;

  g_type_class_add_private (klass, sizeof (MojitoServicePrivate));

  object_class->get_property = mojito_service_get_property;
  object_class->set_property = mojito_service_set_property;
  object_class->dispose = mojito_service_dispose;

  pspec = g_param_spec_object ("core",
                               "core",
                               "The daemon's warp core",
                               MOJITO_TYPE_CORE,
                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
  g_object_class_install_property (object_class, PROP_CORE, pspec);
}

static void
mojito_service_init (MojitoService *self)
{
}

const char *
mojito_service_get_name (MojitoService *service)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_val_if_fail (service_class->get_name, NULL);

  return service_class->get_name (service);
}

MojitoCore *
mojito_service_get_core (MojitoService *service)
{
  return GET_PRIVATE(service)->core;
}

void
mojito_service_update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  MojitoServiceClass *service_class = MOJITO_SERVICE_GET_CLASS (service);

  g_return_if_fail (service_class->update);

  service_class->update (service, callback, user_data);
}

static char *
get_cache_name (MojitoService *service)
{
  char *path, *filename;

  g_assert (service);

  /* TODO: use GIO */

  path = g_build_filename (g_get_user_cache_dir (),
                           "mojito",
                           "cache",
                           NULL);
  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    int err;
    err = g_mkdir_with_parents (path, 0777);
    if (err)
      g_message ("Cannot create cache directory: %s", g_strerror (err));
  }

  filename = g_build_filename (path,
                               mojito_service_get_name (service),
                               NULL);
  g_free (path);

  return filename;
}

static void
cache_set (gpointer data, gpointer user_data)
{
  MojitoItem *item = data;
  GKeyFile *keys = user_data;
  const char *group, *key, *value;
  GHashTableIter iter;

  group = mojito_item_get (item, "id");
  if (group == NULL)
    return;

  g_hash_table_iter_init (&iter, mojito_item_peek_hash (item));
  while (g_hash_table_iter_next (&iter, (gpointer)&key, (gpointer)&value)) {
    g_key_file_set_string (keys, group, key, value);
  }
}

void
mojito_service_cache_items (MojitoService *service, MojitoSet *set)
{
  char *filename;

  g_return_if_fail (MOJITO_IS_SERVICE (service));

  filename = get_cache_name (service);

  if (set == NULL || mojito_set_is_empty (set)) {
    g_remove (filename);
  } else {
    GKeyFile *keys;
    char *data;
    GError *error = NULL;

    keys = g_key_file_new ();
    mojito_set_foreach (set, cache_set, keys);

    data = g_key_file_to_data (keys, NULL, NULL);
    if (!g_file_set_contents (filename, data, -1, &error)) {
      g_message ("Cannot write cache: %s", error->message);
      g_error_free (error);
    }
    g_free (data);
  }

  g_free (filename);
}

static MojitoItem *
cache_load (MojitoService *service, GKeyFile *keyfile, const char *group)
{
  MojitoItem *item = NULL;
  char **keys;
  int i, count;

  keys = g_key_file_get_keys (keyfile, group, &count, NULL);
  if (keys) {
    item = mojito_item_new ();
    mojito_item_set_service (item, service);
    for (i = 0; i < count; i++) {
      mojito_item_take (item, keys[i], g_key_file_get_string (keyfile, group, keys[i], NULL));
    }
  }
  g_strfreev (keys);
  return item;
}

MojitoSet *
mojito_service_load_cache (MojitoService *service)
{
  char *filename;
  GKeyFile *keys;
  MojitoSet *set = NULL;

  g_return_val_if_fail (MOJITO_IS_SERVICE (service), NULL);

  keys = g_key_file_new ();

  filename = get_cache_name (service);

  if (g_key_file_load_from_file (keys, filename, G_KEY_FILE_NONE, NULL)) {
    char **groups;
    int i, count;

    groups = g_key_file_get_groups (keys, &count);
    if (count) {
      set = mojito_item_set_new ();
      for (i = 0; i < count; i++) {
        mojito_set_add (set, (GObject*)cache_load (service, keys, groups[i]));
      }
    }

    g_strfreev (groups);

  }

  g_free (filename);

  return set;
}
