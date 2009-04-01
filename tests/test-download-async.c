#include <glib-object.h>
#include <mojito/mojito-web.h>

static GMainLoop *loop;

static void
callback (const char *url, char *filename, gpointer user_data)
{
  g_print ("\nGot local file %s\n", filename);
  g_free (filename);

  g_main_loop_quit (loop);
  g_main_loop_unref (loop);
  loop = NULL;
}

static gboolean
progress (gpointer data)
{
  g_print (".");
  return TRUE;
}

int
main (int argc, char **argv)
{
  if (argc != 2) {
    g_print ("$ test-download-async <URL>\n");
    return 1;
  }

  g_thread_init (NULL);
  g_type_init ();

  loop = g_main_loop_new (NULL, TRUE);

  mojito_web_download_image_async (argv[1], callback, NULL);

  /* If the file was cached then we didn't need to enter the main loop, so check
     that it still exists before running it. */
  if (loop) {
    g_idle_add (progress, NULL);
    g_main_loop_run (loop);
  }

  return 0;
}
