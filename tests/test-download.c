#include <glib-object.h>
#include <mojito/mojito-web.h>

int
main (int argc, char **argv)
{
  char *file;

  if (argc != 2) {
    g_print ("$ test-download <URL>\n");
    return 1;
  }

  g_thread_init (NULL);
  g_type_init ();

  file = mojito_web_download_image (argv[1]);
  g_print ("Got local file %s\n", file);

  return 0;
}
