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

#include <stdlib.h>
#include "skeleton.h"
#include <mojito/mojito-item.h>
#include <mojito/mojito-set.h>
#include <mojito/mojito-utils.h>

G_DEFINE_TYPE (MojitoServiceSkeleton, mojito_service_skeleton, MOJITO_TYPE_SERVICE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SERVICE_SKELETON, MojitoServiceSkeletonPrivate))

struct _MojitoServiceSkeletonPrivate {
  int dummy;
};

static void
update (MojitoService *service, MojitoServiceDataFunc callback, gpointer user_data)
{
  //MojitoServiceSkeleton *skeleton = (MojitoServiceSkeleton*)service;
}

static const char *
mojito_service_skeleton_get_name (MojitoService *service)
{
  return "skeleton";
}

static void
mojito_service_skeleton_dispose (GObject *object)
{
  //MojitoServiceSkeletonPrivate *priv = MOJITO_SERVICE_SKELETON (object)->priv;

  G_OBJECT_CLASS (mojito_service_skeleton_parent_class)->dispose (object);
}

static void
mojito_service_skeleton_finalize (GObject *object)
{
  //MojitoServiceSkeletonPrivate *priv = MOJITO_SERVICE_SKELETON (object)->priv;

  G_OBJECT_CLASS (mojito_service_skeleton_parent_class)->finalize (object);
}

static void
mojito_service_skeleton_class_init (MojitoServiceSkeletonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoServiceClass *service_class = MOJITO_SERVICE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoServiceSkeletonPrivate));

  object_class->dispose = mojito_service_skeleton_dispose;
  object_class->finalize = mojito_service_skeleton_finalize;

  service_class->get_name = mojito_service_skeleton_get_name;
  service_class->update = update;
}

static void
mojito_service_skeleton_init (MojitoServiceSkeleton *self)
{
  self->priv = GET_PRIVATE (self);
}
