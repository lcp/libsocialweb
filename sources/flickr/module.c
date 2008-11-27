#include "mojito-module.h"
#include "mojito-source-flickr.h"

const gchar *
mojito_module_get_name (void)
{
  return "flickr";
}

const GType
mojito_module_get_type (void)
{
  return MOJITO_TYPE_SOURCE_FLICKR;
}
