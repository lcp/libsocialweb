#include "mojito-module.h"
#include "lastfm.h"

const gchar *
mojito_module_get_name (void)
{
  return "lastfm";
}

const GType
mojito_module_get_type (void)
{
  return MOJITO_TYPE_SOURCE_LASTFM;
}
