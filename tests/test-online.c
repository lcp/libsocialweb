#include <glib-object.h>
#include <mojito/mojito-online.h>

int
main (int argc, char **argv)
{
  g_type_init ();
  if (mojito_is_online ()) {
    g_print ("Online\n");
  } else {
    g_print ("Offline\n");
  }
}
