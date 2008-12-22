#include <glib.h>
#include <time.h>

/* TODO: these are not used at the moment */
time_t mojito_time_t_from_string (const char *s);
char * mojito_time_t_to_string (time_t t);
char * mojito_date_to_iso (const char *s);
