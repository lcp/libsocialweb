#include "mojito-module.h"
#include "skeleton.h"

const gchar *
mojito_module_get_name (void)
{
  return "skeleton";
}

const GType
mojito_module_get_type (void)
{
  return MOJITO_TYPE_SOURCE_SKELETON;
}
