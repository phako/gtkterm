/***********************************************************************/
/* gtkterm.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Main program file                                              */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.0 : added call to add_shortcuts()                       */
/*      - 0.98 : all GUI functions moved to widgets.c                  */
/*                                                                     */
/***********************************************************************/

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdlib.h>

#include "widgets.h"
#include "serie.h"
#include "term_config.h"
#include "cmdline.h"
#include "buffer.h"
#include "macros.h"
#include "auto_config.h"

#include <config.h>
#include <glib/gi18n.h>

static void migrate_config (void)
{
    char *old_config_path = NULL;
    char *new_config_path = NULL;
    GFile *file = NULL;
    GFile *new_file = NULL;
    GDataInputStream *stream = NULL;
    GFileInputStream *input_stream = NULL;
    GError *error = NULL;
    char *line = NULL;
    int macro_counter = 0;
    const char macro_regex[] = "^[[:space:]]*macros[[:space:]]*=";
    GRegex *re = g_regex_new (macro_regex, 0, 0, NULL);
    GString *config = g_string_new ("");
    gsize length = 0;

    old_config_path = g_build_filename (g_get_home_dir (), ".gtktermrc", NULL);
    file = g_file_new_for_commandline_arg (old_config_path);

    new_config_path = g_build_filename (g_get_user_config_dir (), "gtktermrc", NULL);
    new_file = g_file_new_for_commandline_arg (new_config_path);

    // Do nothing if there is no old file or already a new file. To prevent
    // overwriting a previously converted file
    if (!g_file_query_exists (file, NULL) || g_file_query_exists (new_file, NULL)) {
        goto out;
    }

    input_stream = g_file_read (file, NULL, &error);
    stream = g_data_input_stream_new (G_INPUT_STREAM (input_stream));
    for (line = g_data_input_stream_read_line (stream, &length, NULL, &error);
         line != NULL;
         line = g_data_input_stream_read_line (stream, &length, NULL, &error)) {
        if (g_str_has_prefix (line, "["))
            macro_counter = 0;

        if (g_regex_match (re, line, 0, NULL)) {
            char *prefix = NULL;
            char *old_line = NULL;

            prefix = g_strdup_printf ("macros%d=", macro_counter++);
            old_line = line;

            line = g_regex_replace_literal (re, line, -1, 0, prefix, 0, &error);
            if (line != old_line)
                g_free (old_line);

            g_free (prefix);
        }

        g_string_append (config, line);
        g_string_append_c (config, '\n');
    }

    g_file_set_contents (new_config_path, config->str, config->len, &error);

    g_input_stream_close (G_INPUT_STREAM (stream), NULL, &error);
    g_file_delete (file, NULL, &error);

out:
    g_clear_object (&stream);
    g_clear_object (&input_stream);
    g_clear_pointer (&old_config_path, g_free);
    g_clear_object (&file);
    g_clear_pointer (&re, g_regex_unref);
    g_clear_pointer (&new_config_path, g_free);
    g_clear_object (&new_file);
}

int main(int argc, char *argv[])
{
  gchar *message;

  migrate_config ();

  config_file = g_build_filename (g_get_user_config_dir(), "gtktermrc", NULL);

  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);

  gtk_init(&argc, &argv);

  create_buffer();

  create_main_window();

  if(read_command_line(argc, argv, NULL) < 0)
    {
      delete_buffer();
      exit(1);
    }

  Config_port();

  message = get_port_string();
  Set_window_title(message);
  Set_status_message(message);
  g_free(message);

  add_shortcuts();

  set_view(ASCII_VIEW);

  gtk_main();

  delete_buffer();

  Close_port_and_remove_lockfile();

  return 0;
}
