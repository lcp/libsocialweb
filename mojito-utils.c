#include "mojito-utils.h"
#include <libsoup/soup.h>

time_t
mojito_time_t_from_string (const char *s)
{
  SoupDate *date;
  time_t t;

  date = soup_date_new_from_string (s);
  t = soup_date_to_time_t (date);
  soup_date_free (date);
  
  return t;
}

char *
mojito_time_t_to_string (time_t t)
{
  SoupDate *date;
  char *s;
  
  date = soup_date_new_from_time_t (t);
  s = soup_date_to_string (date, SOUP_DATE_HTTP);
  soup_date_free (date);
  
  return s;
}
