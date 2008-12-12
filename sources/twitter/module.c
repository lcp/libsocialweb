#include "mojito-module.h"
#include "twitter.h"

const gchar *
mojito_module_get_name (void)
{
  return "twitter";
}

const GType
mojito_module_get_type (void)
{
  return MOJITO_TYPE_SOURCE_TWITTER;
}
