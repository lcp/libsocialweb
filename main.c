#include "mojito-core.h"

int
main (int argc, char **argv)
{
  MojitoCore *core;

  g_thread_init (NULL);
  g_type_init ();

  core = mojito_core_new ();

  mojito_core_run (core);
  
  g_object_unref (core);
  
  return 0;
}
