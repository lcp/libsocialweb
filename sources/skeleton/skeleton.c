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

G_DEFINE_TYPE (MojitoSourceSkeleton, mojito_source_skeleton, MOJITO_TYPE_SOURCE)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MOJITO_TYPE_SOURCE_SKELETON, MojitoSourceSkeletonPrivate))

struct _MojitoSourceSkeletonPrivate {
  int dummy;
};

static void
update (MojitoSource *source, MojitoSourceDataFunc callback, gpointer user_data)
{
  //MojitoSourceSkeleton *skeleton = (MojitoSourceSkeleton*)source;
}

static const char *
mojito_source_skeleton_get_name (MojitoSource *source)
{
  return "skeleton";
}

static void
mojito_source_skeleton_dispose (GObject *object)
{
  //MojitoSourceSkeletonPrivate *priv = MOJITO_SOURCE_SKELETON (object)->priv;

  G_OBJECT_CLASS (mojito_source_skeleton_parent_class)->dispose (object);
}

static void
mojito_source_skeleton_finalize (GObject *object)
{
  //MojitoSourceSkeletonPrivate *priv = MOJITO_SOURCE_SKELETON (object)->priv;

  G_OBJECT_CLASS (mojito_source_skeleton_parent_class)->finalize (object);
}

static void
mojito_source_skeleton_class_init (MojitoSourceSkeletonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  MojitoSourceClass *source_class = MOJITO_SOURCE_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MojitoSourceSkeletonPrivate));

  object_class->dispose = mojito_source_skeleton_dispose;
  object_class->finalize = mojito_source_skeleton_finalize;

  source_class->get_name = mojito_source_skeleton_get_name;
  source_class->update = update;
}

static void
mojito_source_skeleton_init (MojitoSourceSkeleton *self)
{
  self->priv = GET_PRIVATE (self);
}
