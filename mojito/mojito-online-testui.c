/*
 * Mojito - social data store
 * Copyright (C) 2008 - 2009 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>
#include <gtk/gtk.h>

static GtkWidget *button;

static void
on_toggled (GtkToggleButton *button, gpointer user_data)
{
  gboolean state;

  state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  gtk_button_set_label (GTK_BUTTON (button), state ? "Go Offline" : "Go Online");

  emit_notify (state);
}

static gboolean
online_init (void)
{
  GtkWidget *window;

  if (button)
    return TRUE;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Mojito Online State");

  button = gtk_toggle_button_new_with_label ("Go Online");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);
  g_signal_connect (button, "toggled", G_CALLBACK (on_toggled), NULL);
  gtk_container_add (GTK_CONTAINER (window), button);

  gtk_widget_show_all (window);

  return TRUE;
}

gboolean
mojito_is_online (void)
{
  online_init ();

  return gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
}
