#include "mojito-module.h"
#include "mojito-source-dummy.h"

const gchar *
mojito_module_get_name (void)
{
  return "dummy";
}

const GType
mojito_module_get_type (void)
{
  return mojito_source_dummy_get_type();
}
