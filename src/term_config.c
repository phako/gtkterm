/***********************************************************************/
/* term_config.c                                                            */
/* --------                                                            */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Configuration of the serial port                               */
/*                                                                     */
/*   ChangeLog                                                         */
/*      - 0.99.7 : Refactor to use newer gtk widgets                   */
/*                 Add ability to use arbitrary baud                   */
/*                 Add rs458 capability - Marc Le Douarain             */
/*                 Remove auto cr/lf stuff - (use macros instead)      */
/*      - 0.99.5 : Make the combo list for the device editable         */
/*      - 0.99.3 : Configuration for VTE terminal                      */
/*      - 0.99.2 : Internationalization                                */
/*      - 0.99.1 : fixed memory management bug                         */
/*                 test if there are devices found                     */
/*      - 0.99.0 : fixed enormous memory management bug ;-)            */
/*                 save / read macros                                  */
/*      - 0.98.5 : font saved in configuration                         */
/*                 bug fixed in memory management                      */
/*                 combos set to non editable                          */
/*      - 0.98.3 : configuration file                                  */
/*      - 0.98.2 : autodetect existing devices                         */
/*      - 0.98 : added devfs devices                                   */
/*                                                                     */
/***********************************************************************/

#include <gtk/gtk.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vte/vte.h>
#include <glib/gi18n.h>

#include "serie.h"
#include "term_config.h"
#include "widgets.h"
#include "macros.h"
#include "i18n.h"
#include "config.h"


#define DEVICE_NUMBERS_TO_CHECK 12

const gchar *devices_to_check[] = {
    "/dev/ttyS%d",
    "/dev/tts/%d",
    "/dev/ttyUSB%d",
    "/dev/ttyACM%d",
    "/dev/usb/tts/%d",
    NULL
};

gchar *config_file;

struct configuration_port port_conf;
display_config_t term_conf;

GtkWidget *Entry;

gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur);
void read_font_button(GtkFontButton *fontButton);
void Hard_default_configuration(void);
void Copy_configuration(GKeyFile *, const char *section);

static void Select_config(gchar *, void *);
static void Save_config_file(void);
static void load_config(GtkDialog *, gint, GtkTreeSelection *);
static void delete_config(GtkDialog *, gint, GtkTreeSelection *);
static void save_config(GtkDialog *, gint, GtkWidget *);
void really_save_config(GKeyFile *config, const char *section);
static gint remove_section(gchar *, gchar *);
static void Curseur_OnOff(GtkWidget *, gpointer);
static void Selec_couleur(GdkRGBA *, gfloat, gfloat, gfloat);
void config_fg_color(GtkWidget *button, gpointer data);
void config_bg_color(GtkWidget *button, gpointer data);
static void scrollback_set(GtkSpinButton *spin_button, gpointer data);

extern GtkWidget *display;

void Config_Port_Fenetre(GtkAction *action, gpointer data)
{
    GtkBuilder *builder;
    GtkDialog *dialog;
    GtkWidget *combo;
    GtkWidget *entry;
    GList *device_list = NULL;
    int device_list_length = 0;
    GList *it = NULL;
    char *rate = NULL;
    struct stat my_stat;
    int result = GTK_RESPONSE_CANCEL;

    const gchar **dev = NULL;
    guint i;

    for(dev = devices_to_check; *dev != NULL; dev++)
    {
        for(i = 0; i < DEVICE_NUMBERS_TO_CHECK; i++)
        {
            gchar *device_name = NULL;

            device_name = g_strdup_printf(*dev, i);
            if (stat(device_name, &my_stat) == 0) {
                device_list = g_list_prepend (device_list, device_name);
                device_list_length++;
            }
            else
                g_free (device_name);
        }
    }

    device_list = g_list_reverse (device_list);

    if (device_list == NULL)
    {
        show_message(_("No serial devices found!\n\n"
                    "Searched the following paths:\n"
                    "\t/dev/ttyS*\n\t/dev/tts/*\n\t/dev/ttyUSB*\n\t/dev/usb/tts/*\n\n"
                    "Enter a different device path in the 'Port' box.\n"), MSG_WRN);
    }

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/settings-port.ui");
    dialog = GTK_DIALOG (gtk_builder_get_object (builder, "dialog-settings-port"));
    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-device"));

    for (it = device_list; it != NULL; it = it->next) {
        gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo),
                                        (const gchar *) it->data);
    }

    g_list_free_full (device_list, g_free);

    /* Set values on first page */
    if (port_conf.port != NULL && port_conf.port[0] != '\0') {
        entry = gtk_bin_get_child (GTK_BIN (combo));

        gtk_entry_set_text (GTK_ENTRY (entry), port_conf.port);
    } else {
        gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
    }

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-baud-rate"));
    rate = g_strdup_printf ("%d", port_conf.vitesse);
    entry = gtk_bin_get_child (GTK_BIN (combo));

    if (port_conf.vitesse == 0) {
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), "9600");
    } else {
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate)) {
            gtk_entry_set_text (GTK_ENTRY (entry), rate);
        }
    }
    g_free (rate);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-parity"));
    rate = g_strdup_printf ("%d", port_conf.parite);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate);
    g_free (rate);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-bits"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo), port_conf.bits);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-stopbits"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo), port_conf.stops);

    combo = GTK_WIDGET (gtk_builder_get_object (builder, "combo-flow"));
    rate = g_strdup_printf ("%d", port_conf.flux);
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), rate);
    g_free (rate);

    /* Set values on second page */
    {
        GtkWidget *spin;
        spin = GTK_WIDGET (gtk_builder_get_object (builder, "spin-eol-delay"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gfloat) port_conf.delai);

        combo = GTK_WIDGET (gtk_builder_get_object (builder, "check-use-wait-char"));

        g_signal_connect (G_OBJECT (combo), "toggled", G_CALLBACK (Grise_Degrise), spin);

        Entry = combo = GTK_WIDGET (gtk_builder_get_object (builder, "entry-wait-char"));

        if (port_conf.car != -1) {
            gtk_entry_set_text (GTK_ENTRY (combo), &(port_conf.car));
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (combo), TRUE);
        }
    }

    /* Set values on third page */
    {
        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-on"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo),
                                   (gfloat) port_conf.rs485_rts_time_before_transmit);

        combo = GTK_WIDGET (gtk_builder_get_object (builder, "spin-rs485-off"));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (combo),
                                   (gfloat) port_conf.rs485_rts_time_after_transmit);
    }

    result = gtk_dialog_run (dialog);
    gtk_widget_hide (GTK_WIDGET (dialog));
    if (result == GTK_RESPONSE_OK) {
        Lis_Config (builder);
    }

    g_object_unref (builder);

    gtk_widget_destroy (GTK_WIDGET (dialog));
}

gint Lis_Config(GtkBuilder *builder)
{
    GObject *widget;
    gchar *message;

    widget = gtk_builder_get_object (builder, "combo-device");
    message = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(widget));
    strcpy(port_conf.port, message);
    g_free(message);

    widget = gtk_builder_get_object (builder, "combo-baud-rate");
    message = (char *) gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));
    port_conf.vitesse = atoi(message);

    widget = gtk_builder_get_object (builder, "spin-bits");
    port_conf.bits = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

    widget = gtk_builder_get_object (builder, "spin-eol-delay");
    port_conf.delai = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "spin-rs485-on");
    port_conf.rs485_rts_time_before_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "spin-rs485-off");
    port_conf.rs485_rts_time_after_transmit = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

    widget = gtk_builder_get_object (builder, "combo-parity");
    message = (char *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
    port_conf.parite = atoi (message);

    widget = gtk_builder_get_object (builder, "spin-bits");
    port_conf.stops = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

    widget = gtk_builder_get_object (builder, "combo-flow");
    message = (char *)gtk_combo_box_get_active_id(GTK_COMBO_BOX(widget));
    port_conf.flux = atoi (message);

    widget = gtk_builder_get_object (builder, "check-use-wait-char");
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
    {
        widget = gtk_builder_get_object (builder, "entry-wait-char");
        port_conf.car = *gtk_entry_get_text(GTK_ENTRY(widget));
        port_conf.delai = 0;
    }
    else
        port_conf.car = -1;

    Config_port();

    message = get_port_string();
    Set_status_message(message);
    Set_window_title(message);
    g_free(message);

    return FALSE;
}

gint Grise_Degrise(GtkWidget *bouton, gpointer pointeur)
{
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bouton)))
    {
	gtk_widget_set_sensitive(GTK_WIDGET(Entry), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(pointeur), FALSE);
    }
    else
    {
	gtk_widget_set_sensitive(GTK_WIDGET(Entry), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(pointeur), TRUE);
    }
    return FALSE;
}

void read_font_button(GtkFontButton *fontButton)
{
    const char *font_name = gtk_font_button_get_font_name (fontButton);
    PangoFontDescription *old_font = term_conf.font;

    if (font_name == NULL) {
        return;
    }

    term_conf.font = pango_font_description_from_string (font_name);
    if (term_conf.font != NULL) {
        g_clear_pointer (&old_font, pango_font_description_free);
        vte_terminal_set_font (VTE_TERMINAL(display), term_conf.font);
    } else {
        term_conf.font = old_font;
    }
}


void select_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Load configuration"), G_CALLBACK(load_config));
}

void save_config_callback(GtkAction *action, gpointer data)
{
	Save_config_file();
}

void delete_config_callback(GtkAction *action, gpointer data)
{
	Select_config(_("Delete configuration"), G_CALLBACK(delete_config));
}

void Select_config(gchar *title, void *callback)
{
    GtkWidget *dialog;
    GtkWidget *content_area;
    guint i;

    GtkWidget *Frame, *Scroll, *Liste, *Label;
    gchar *texte_label;

    GtkListStore *Modele_Liste;
    GtkTreeIter iter_Liste;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *Colonne;
    GtkTreeSelection *Selection_Liste;
    GKeyFile *config;
    GError *error = NULL;
    char **groups = NULL;
    gsize groups_size = 0;

    enum
    {
	N_texte,
	N_COLONNES
    };

    /* Parse the config file */

    config = g_key_file_new ();
    if (!g_key_file_load_from_file (config, config_file, G_KEY_FILE_NONE, &error)) {
        g_debug ("Error reading configuration file: %s", error->message);
        g_error_free (error);
        show_message(_("Cannot read configuration file!\n"), MSG_ERR);
    }

    else
    {
	gchar *Titre[]= {_("Configurations")};

	dialog = gtk_dialog_new_with_buttons (title,
					      NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
                          _("_Cancel"),
					      GTK_RESPONSE_NONE,
                          _("_OK"),
					      GTK_RESPONSE_ACCEPT,
					      NULL);

	content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	Modele_Liste = gtk_list_store_new(N_COLONNES, G_TYPE_STRING);

	Liste = gtk_tree_view_new_with_model(GTK_TREE_MODEL(Modele_Liste));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(Liste), N_texte);

	Selection_Liste = gtk_tree_view_get_selection(GTK_TREE_VIEW(Liste));
	gtk_tree_selection_set_mode(Selection_Liste, GTK_SELECTION_SINGLE);

	Frame = gtk_frame_new(NULL);

	Scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(Scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(Frame), Scroll);
	gtk_container_add(GTK_CONTAINER(Scroll), Liste);

	renderer = gtk_cell_renderer_text_new();

	g_object_set(G_OBJECT(renderer), "xalign", (gfloat)0.5, NULL);
	Colonne = gtk_tree_view_column_new_with_attributes(Titre[0], renderer, "text", 0, NULL);
	gtk_tree_view_column_set_sort_column_id(Colonne, 0);

	Label=gtk_label_new("");
	texte_label = g_strdup_printf("<span weight=\"bold\" style=\"italic\">%s</span>", Titre[0]);
	gtk_label_set_markup(GTK_LABEL(Label), texte_label);
	g_free(texte_label);
	gtk_tree_view_column_set_widget(GTK_TREE_VIEW_COLUMN(Colonne), Label);
	gtk_widget_show(Label);

	gtk_tree_view_column_set_alignment(GTK_TREE_VIEW_COLUMN(Colonne), 0.5f);
	gtk_tree_view_column_set_resizable(GTK_TREE_VIEW_COLUMN(Colonne), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(Liste), Colonne);


    groups = g_key_file_get_groups (config, &groups_size);

	for(i = 0; i < groups_size; i++)
	{
	    gtk_list_store_append(Modele_Liste, &iter_Liste);
	    gtk_list_store_set(Modele_Liste, &iter_Liste, N_texte, groups[i], -1);
	}

    g_clear_pointer (&groups, g_strfreev);
    g_clear_pointer (&config, g_key_file_unref);

	gtk_widget_set_size_request(GTK_WIDGET(dialog), 200, 200);

	g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK (callback), GTK_TREE_SELECTION(Selection_Liste));
	g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

	gtk_box_pack_start (GTK_BOX (content_area), Frame, TRUE, TRUE, 0);

	gtk_widget_show_all (dialog);
    }
}

void Save_config_file(void)
{
    GtkWidget *dialog, *content_area, *label, *box, *entry;

    dialog = gtk_dialog_new_with_buttons (_("Save configuration"),
					  NULL,
					  GTK_DIALOG_DESTROY_WITH_PARENT,
                      _("_Cancel"),
					  GTK_RESPONSE_NONE,
                      _("_OK"),
					  GTK_RESPONSE_ACCEPT,
					  NULL);
    content_area = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);

    label = gtk_label_new(_("Configuration name: "));

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), "default");
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, TRUE, TRUE, 0);

    //validate input text (alpha-numeric only)
    g_signal_connect(GTK_ENTRY(entry),
		     "insert-text",
		     G_CALLBACK(check_text_input), isalnum);
    g_signal_connect(GTK_WIDGET(dialog), "response", G_CALLBACK(save_config), GTK_ENTRY(entry));
    g_signal_connect_swapped(GTK_WIDGET(dialog), "response", G_CALLBACK(gtk_widget_destroy), GTK_WIDGET(dialog));

    gtk_box_pack_start(GTK_BOX(content_area), box, TRUE, FALSE, 5);

    gtk_widget_show_all (dialog);
}

void really_save_config(GKeyFile *config, const char *section)
{
    gchar *string = NULL;
    GError *error = NULL;

    Copy_configuration(config, section);
    g_key_file_save_to_file (config, config_file, &error);

    string = g_strdup_printf(_("Configuration [%s] saved\n"), section);
    show_message(string, MSG_WRN);
    g_free(string);
}

void save_config(GtkDialog *dialog, gint response_id, GtkWidget *edit)
{
	const gchar *config_name;
    GKeyFile *config;
    GError *error = NULL;

	if(response_id == GTK_RESPONSE_ACCEPT)
	{
        config = g_key_file_new ();
        if (!g_key_file_load_from_file (config, config_file, G_KEY_FILE_NONE, &error)) {
            g_debug ("Failed to load config: %s", error->message);
            g_error_free (error);
            g_key_file_unref (config);
            return;
        }


        config_name = gtk_entry_get_text(GTK_ENTRY(edit));

        if (g_key_file_has_group(config, config_name)) {
            GtkWidget *message_dialog;
            message_dialog = gtk_message_dialog_new_with_markup(GTK_WINDOW(Fenetre),
                    GTK_DIALOG_DESTROY_WITH_PARENT,
                    GTK_MESSAGE_QUESTION,
                    GTK_BUTTONS_NONE,
                    _("<b>Section [%s] already exists.</b>\n\nDo you want to overwrite it ?"),
                    config_name);

            gtk_dialog_add_buttons(GTK_DIALOG(message_dialog),
                                       _("_Cancel"),
                    GTK_RESPONSE_NONE,
                                       _("_OK"),
                    GTK_RESPONSE_ACCEPT,
                    NULL);

            if (gtk_dialog_run(GTK_DIALOG(message_dialog)) == GTK_RESPONSE_ACCEPT)
                really_save_config(config, config_name);

            gtk_widget_destroy(message_dialog);
        }
        else
            really_save_config(config, config_name);
    }
}

void load_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
    GtkTreeIter iter;
    GtkTreeModel *Modele;
    gchar *txt, *message;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
	if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
	{
	    gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, &txt, -1);
	    Load_configuration_from_file(txt);
	    Verify_configuration();
	    Config_port();
	    add_shortcuts();

	    message = get_port_string();
	    Set_status_message(message);
	    Set_window_title(message);
	    g_free(message);
	}
    }
}

void delete_config(GtkDialog *dialog, gint response_id, GtkTreeSelection *Selection_Liste)
{
    GtkTreeIter iter;
    GtkTreeModel *Modele;
    gchar *txt;

    if(response_id == GTK_RESPONSE_ACCEPT)
    {
	if(gtk_tree_selection_get_selected(Selection_Liste, &Modele, &iter))
	{
	    gtk_tree_model_get(GTK_TREE_MODEL(Modele), &iter, 0, (gint *)&txt, -1);
	    if(remove_section(config_file, txt) == -1)
		show_message(_("Cannot delete section!"), MSG_ERR);
	}
    }
}

gint Load_configuration_from_file(const gchar *section)
{
    GKeyFile *config_object = NULL;
    GError *error = NULL;
    gchar *str = NULL;
    int value = 0;

    gchar *string = NULL;

    config_object = g_key_file_new ();
    if (!g_key_file_load_from_file (config_object, config_file, G_KEY_FILE_NONE, &error)) {
        g_debug ("Failed to load configuration file: %s", error->message);
        g_error_free (error);
        g_key_file_unref (config_object);

        return -1;
    }

    if (!g_key_file_has_group (config_object, section)) {
        string = g_strdup_printf(_("No section \"%s\" in configuration file\n"), section);
        show_message(string, MSG_ERR);
        g_free(string);
        g_key_file_unref (config_object);
        return -1;
   }

    Hard_default_configuration();
    str = g_key_file_get_string (config_object, section, "port", NULL);
    if (str != NULL) {
        strncpy (port_conf.port, str, sizeof (port_conf.port) - 1);
        g_free (str);
    }

    value = g_key_file_get_integer (config_object, section, "speed", NULL);
    if (value != 0) {
        port_conf.vitesse = value;
    }

    value = g_key_file_get_integer (config_object, section, "bits", NULL);
    if (value != 0) {
        port_conf.bits = value;
    }

    value = g_key_file_get_integer (config_object, section, "stopbits", NULL);
    if (value != 0) {
        port_conf.stops = value;
    }

    str = g_key_file_get_string (config_object, section, "parity", NULL);
    if (str != NULL) {
        if(!g_ascii_strcasecmp(str, "none"))
            port_conf.parite = 0;
        else if(!g_ascii_strcasecmp(str, "odd"))
            port_conf.parite = 1;
        else if(!g_ascii_strcasecmp(str, "even"))
            port_conf.parite = 2;
        g_free (str);
    }

    str = g_key_file_get_string (config_object, section, "flow", NULL);
    if (str != NULL) {
        if(!g_ascii_strcasecmp(str, "none"))
            port_conf.flux = 0;
        else if(!g_ascii_strcasecmp(str, "xon"))
            port_conf.flux = 1;
        else if(!g_ascii_strcasecmp(str, "rts"))
            port_conf.flux = 2;
        else if(!g_ascii_strcasecmp(str, "rs485"))
            port_conf.flux = 3;
        g_free (str);
    }

    port_conf.delai = g_key_file_get_integer (config_object, section, "wait_delay", NULL);

    value = g_key_file_get_integer (config_object, section, "wait_char", NULL);
    if (value != 0) {
        port_conf.car = (signed char) value;
    } else {
        port_conf.car = -1;
    }
    port_conf.rs485_rts_time_before_transmit = g_key_file_get_integer (config_object, section, "rs485_rts_time_before_tx", NULL);
    port_conf.rs485_rts_time_after_transmit = g_key_file_get_integer (config_object, section, "rs485_rts_time_after_tx", NULL);
    port_conf.echo = g_key_file_get_boolean (config_object, section, "echo", NULL);
    port_conf.crlfauto = g_key_file_get_boolean (config_object, section, "crlfauto", NULL);

        g_clear_pointer (&term_conf.font, pango_font_description_free);
    str = g_key_file_get_string (config_object, section, "font", NULL);
    term_conf.font = pango_font_description_from_string (str);
    g_free (str);

    /* FIXME: Fix macros */
    remove_shortcuts ();

    term_conf.show_cursor = g_key_file_get_boolean (config_object, section, "term_show_cursor", NULL);
    term_conf.rows = g_key_file_get_integer (config_object, section, "term_rows", NULL);
    term_conf.columns = g_key_file_get_integer (config_object, section, "term_columns", NULL);

    value = g_key_file_get_integer (config_object, section, "term_scrollback", NULL);
    if (value != 0) {
        term_conf.scrollback = value;
    }

    term_conf.visual_bell = g_key_file_get_boolean (config_object, section, "term_visual_bell", NULL);
    term_conf.foreground_color.red = g_key_file_get_integer (config_object, section, "term_foreground_red", NULL);
    term_conf.foreground_color.green = g_key_file_get_integer (config_object, section, "term_foreground_green", NULL);
    term_conf.foreground_color.blue = g_key_file_get_integer (config_object, section, "term_foreground_blue", NULL);
    term_conf.background_color.red = g_key_file_get_integer (config_object, section, "term_background_red", NULL);
    term_conf.background_color.green = g_key_file_get_integer (config_object, section, "term_background_green", NULL);
    term_conf.background_color.blue = g_key_file_get_integer (config_object, section, "term_background_blue", NULL);

    if (term_conf.rows == 0 || term_conf.columns == 0) {
        term_conf.show_cursor = TRUE;
        term_conf.rows = 80;
        term_conf.columns = 25;
        term_conf.scrollback = DEFAULT_SCROLLBACK;
        term_conf.visual_bell = FALSE;

        term_conf.foreground_color.red = 43253;
        term_conf.foreground_color.green = 43253;
        term_conf.foreground_color.blue = 43253;

        term_conf.background_color.red = 0;
        term_conf.background_color.green = 0;
        term_conf.background_color.blue = 0;
    }

    vte_terminal_set_font (VTE_TERMINAL(display), term_conf.font);

    vte_terminal_set_size (VTE_TERMINAL(display), term_conf.rows, term_conf.columns);
    vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
    vte_terminal_set_color_foreground (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.foreground_color);
    vte_terminal_set_color_background (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.background_color);
    gtk_widget_queue_draw(display);
    return 0;

}

void Verify_configuration(void)
{
    gchar *string = NULL;

    switch(port_conf.vitesse)
    {
	case 300:
	case 600:
	case 1200:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
	    break;

	default:
	    string = g_strdup_printf(_("Unknown rate: %d baud\nMay not be supported by all hardware"), port_conf.vitesse);
	    show_message(string, MSG_ERR);
	    g_free(string);
    }

    if(port_conf.stops != 1 && port_conf.stops != 2)
    {
	string = g_strdup_printf(_("Invalid number of stop-bits: %d\nFalling back to default number of stop-bits number: %d\n"), port_conf.stops, DEFAULT_STOP);
	show_message(string, MSG_ERR);
	port_conf.stops = DEFAULT_STOP;
	g_free(string);
    }

    if(port_conf.bits < 5 || port_conf.bits > 8)
    {
	string = g_strdup_printf(_("Invalid number of bits: %d\nFalling back to default number of bits: %d\n"), port_conf.bits, DEFAULT_BITS);
	show_message(string, MSG_ERR);
	port_conf.bits = DEFAULT_BITS;
	g_free(string);
    }

    if(port_conf.delai < 0 || port_conf.delai > 500)
    {
	string = g_strdup_printf(_("Invalid delay: %d ms\nFalling back to default delay: %d ms\n"), port_conf.delai, DEFAULT_DELAY);
	show_message(string, MSG_ERR);
	port_conf.delai = DEFAULT_DELAY;
	g_free(string);
    }

    if(term_conf.font == NULL) {
        term_conf.font = pango_font_description_from_string (DEFAULT_FONT);
    }

}

gint Check_configuration_file(void)
{
    struct stat my_stat;
    gchar *string = NULL;

    /* is configuration file present ? */
    if(stat(config_file, &my_stat) == 0)
    {
	/* If bad configuration file, fallback to _hardcoded_ defaults! */
	if(Load_configuration_from_file("default") == -1)
	{
	    Hard_default_configuration();
	    return -1;
	}
    }

    /* if not, create it, with the [default] section */
    else
    {
        GKeyFile *config;
        GError *error = NULL;

	string = g_strdup_printf(_("Configuration file (%s) with\n[default] configuration has been created.\n"), config_file);
	show_message(string, MSG_WRN);
	g_free(string);

    config = g_key_file_new ();
	Hard_default_configuration();
	Copy_configuration(config, "default");
    if (!g_key_file_save_to_file (config, config_file, &error)) {
        g_debug ("Error saving config file: %s", error->message);
        g_error_free (error);
    }
    g_key_file_unref (config);
    }
    return 0;
}

void Hard_default_configuration(void)
{
    strcpy(port_conf.port, DEFAULT_PORT);
    port_conf.vitesse = DEFAULT_SPEED;
    port_conf.parite = DEFAULT_PARITY;
    port_conf.bits = DEFAULT_BITS;
    port_conf.stops = DEFAULT_STOP;
    port_conf.flux = DEFAULT_FLOW;
    port_conf.delai = DEFAULT_DELAY;
    port_conf.rs485_rts_time_before_transmit = DEFAULT_DELAY_RS485;
    port_conf.rs485_rts_time_after_transmit = DEFAULT_DELAY_RS485;
    port_conf.car = DEFAULT_CHAR;
    port_conf.echo = DEFAULT_ECHO;
    port_conf.crlfauto = FALSE;

    term_conf.font = pango_font_description_from_string (DEFAULT_FONT);

    term_conf.show_cursor = TRUE;
    term_conf.rows = 80;
    term_conf.columns = 25;
    term_conf.scrollback = DEFAULT_SCROLLBACK;
    term_conf.visual_bell = TRUE;

    Selec_couleur(&term_conf.foreground_color, 0.66, 0.66, 0.66);
    Selec_couleur(&term_conf.background_color, 0, 0, 0);
}

void Copy_configuration(GKeyFile *configrc, const char *section)
{
    gchar *string = NULL;

    g_key_file_set_string (configrc, section, "port", port_conf.port);
    g_key_file_set_integer (configrc, section, "speed", port_conf.vitesse);
    g_key_file_set_integer (configrc, section,"bits", port_conf.bits);
    g_key_file_set_integer (configrc, section, "stopbits", port_conf.stops);

    switch(port_conf.parite)
    {
    case 0:
        string = g_strdup_printf("none");
        break;
    case 1:
        string = g_strdup_printf("odd");
        break;
    case 2:
        string = g_strdup_printf("even");
        break;
    default:
        string = g_strdup_printf("none");
    }
    g_key_file_set_string (configrc, section, "parity", string);
    g_free(string);

    switch(port_conf.flux)
    {
    case 0:
        string = g_strdup_printf("none");
        break;
    case 1:
        string = g_strdup_printf("xon");
        break;
    case 2:
        string = g_strdup_printf("rts");
        break;
    case 3:
        string = g_strdup_printf("rs485");
        break;
    default:
        string = g_strdup_printf("none");
    }

    g_key_file_set_string (configrc, section, "flow", string);
    g_free(string);

    g_key_file_set_integer (configrc, section, "wait_delay", port_conf.delai);
    g_key_file_set_integer (configrc, section, "wait_char", port_conf.car);
    g_key_file_set_integer (configrc, section, "rs485_rts_time_before_tx",
                            port_conf.rs485_rts_time_before_transmit);
    g_key_file_set_integer (configrc, section, "rs485_rts_time_after_tx",
                            port_conf.rs485_rts_time_after_transmit);

    g_key_file_set_boolean (configrc, section, "echo", port_conf.echo);
    g_key_file_set_boolean (configrc, section, "crlfauto", port_conf.crlfauto);

    string = pango_font_description_to_string (term_conf.font);
    g_key_file_set_string (configrc, section, "font", string);
    g_free(string);

    /* FIXME: Fix macros! */
#if 0
    macros = get_shortcuts(&size);
    for(i = 0; i < size; i++)
    {
	string = g_strdup_printf("%s::%s", macros[i].shortcut, macros[i].action);
	cfgStoreValue(cfg, "macros", string, CFG_INI, pos);
	g_free(string);
    }
#endif

    g_key_file_set_boolean (configrc, section, "term_show_cursor", term_conf.show_cursor);
    g_key_file_set_integer (configrc, section, "term_show_rows", term_conf.rows);
    g_key_file_set_integer (configrc, section, "term_show_columns", term_conf.columns);
    g_key_file_set_integer (configrc, section, "term_show_scrollback", term_conf.scrollback);
    g_key_file_set_boolean (configrc, section, "term_show_visual_bell", term_conf.visual_bell);

    g_key_file_set_integer (configrc, section, "term_foreground_red", term_conf.foreground_color.red);
    g_key_file_set_integer (configrc, section, "term_foreground_green", term_conf.foreground_color.green);
    g_key_file_set_integer (configrc, section, "term_foreground_blue", term_conf.foreground_color.blue);

    g_key_file_set_integer (configrc, section, "term_background_red", term_conf.background_color.red);
    g_key_file_set_integer (configrc, section, "term_background_green", term_conf.background_color.green);
    g_key_file_set_integer (configrc, section, "term_background_blue", term_conf.background_color.blue);
}


gint remove_section(gchar *cfg_file, gchar *section)
{
    FILE *f = NULL;
    char *buffer = NULL;
    char *buf;
    size_t size;
    gchar *to_search;
    size_t i, j, length, sect;

    f = fopen(cfg_file, "r");
    if(f == NULL)
    {
	perror(cfg_file);
	return -1;
    }

    fseek(f, 0L, SEEK_END);
    size = ftell(f);
    rewind(f);

    buffer = g_malloc(size);
    if(buffer == NULL)
    {
	perror("malloc");
	return -1;
    }

    if(fread(buffer, 1, size, f) != size)
    {
	perror(cfg_file);
	fclose(f);
	return -1;
    }

    to_search = g_strdup_printf("[%s]", section);
    length = strlen(to_search);

    /* Search section */
    for(i = 0; i < size - length; i++)
    {
	for(j = 0; j < length; j++)
	{
	    if(to_search[j] != buffer[i + j])
		break;
	}
	if(j == length)
	    break;
    }

    if(i == size - length)
    {
	i18n_printf(_("Cannot find section %s\n"), to_search);
	return -1;
    }

    sect = i;

    /* Search for next section */
    for(i = sect + length; i < size; i++)
    {
	if(buffer[i] == '[')
	    break;
    }

    f = fopen(cfg_file, "w");
    if(f == NULL)
    {
	perror(cfg_file);
	return -1;
    }

    fwrite(buffer, 1, sect, f);
    buf = buffer + i;
    fwrite(buf, 1, size - i, f);
    fclose(f);

    g_free(to_search);
    g_free(buffer);

    return 0;
}

void Config_Terminal(GtkAction *action, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *font_button;
    GtkWidget *check_box;
    GtkWidget *fg_color_button;
    GtkWidget *bg_color_button;
    GtkWidget *scrollback_spin;

    GtkBuilder *builder;

    builder = gtk_builder_new_from_resource ("/org/jensge/GtkTerm/settings-window.ui");

    dialog = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-settings-window"));

    font_button = GTK_WIDGET (gtk_builder_get_object (builder, "font-button"));
    gtk_font_chooser_set_font_desc (GTK_FONT_CHOOSER (font_button), term_conf.font);
    g_signal_connect(GTK_WIDGET(font_button), "font-set", G_CALLBACK(read_font_button), 0);

    check_box = GTK_WIDGET (gtk_builder_get_object (builder, "check-show-cursor"));
    g_signal_connect(GTK_WIDGET(check_box), "toggled", G_CALLBACK(Curseur_OnOff), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_box), term_conf.show_cursor);

    fg_color_button = GTK_WIDGET (gtk_builder_get_object (builder, "color-button-fg"));
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (fg_color_button), &term_conf.foreground_color);
    g_signal_connect (GTK_WIDGET (fg_color_button), "color-set", G_CALLBACK (config_fg_color), NULL);

    bg_color_button = GTK_WIDGET (gtk_builder_get_object (builder, "color-button-bg"));
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (bg_color_button), &term_conf.background_color);
    g_signal_connect (GTK_WIDGET (bg_color_button), "color-set", G_CALLBACK (config_bg_color), NULL);

    scrollback_spin = GTK_WIDGET (gtk_builder_get_object (builder, "spin-scrollback"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (scrollback_spin), term_conf.scrollback);
    g_signal_connect (G_OBJECT(scrollback_spin), "value-changed", G_CALLBACK(scrollback_set), NULL);

    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_hide (dialog);
    gtk_widget_destroy (dialog);
    g_object_unref (builder);
}

void Curseur_OnOff(GtkWidget *Check_Bouton, gpointer data)
{
    term_conf.show_cursor = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Check_Bouton));
}

void Selec_couleur(GdkRGBA *color, gfloat R, gfloat G, gfloat B)
{
	color->red = R;
	color->green = G;
	color->blue = B;
    color->alpha = 1.0;
}

void config_fg_color(GtkWidget *button, gpointer data)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &term_conf.foreground_color);

	vte_terminal_set_color_foreground (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.foreground_color);
	gtk_widget_queue_draw (display);

#if 0
	cfgStoreValue (cfg, "term_foreground_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.foreground_color.green * G_MAXUINT16));
	cfgStoreValue (cfg, "term_foreground_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.foreground_color.blue * G_MAXUINT16));
	cfgStoreValue (cfg, "term_foreground_blue", string, CFG_INI, 0);
	g_free (string);
#endif
}

void config_bg_color(GtkWidget *button, gpointer data)
{
	gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &term_conf.background_color);

	vte_terminal_set_color_background (VTE_TERMINAL(display), (const GdkRGBA *)&term_conf.background_color);
	gtk_widget_queue_draw (display);

#if 0
	cfgStoreValue (cfg, "term_background_red", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.background_color.green * G_MAXUINT16));
	cfgStoreValue (cfg, "term_background_green", string, CFG_INI, 0);
	g_free (string);
	string = g_strdup_printf ("%d", (guint16) (term_conf.background_color.blue * G_MAXUINT16));
	cfgStoreValue (cfg, "term_background_blue", string, CFG_INI, 0);
	g_free (string);
#endif
}


void scrollback_set(GtkSpinButton *spin_button, gpointer data)
{
    int scrollback_value = gtk_spin_button_get_value_as_int (spin_button);
    term_conf.scrollback = scrollback_value;
    vte_terminal_set_scrollback_lines (VTE_TERMINAL(display), term_conf.scrollback);
}

/**
 *  Filter user data entry on a GTK entry
 *
 *  user_data must be a function that takes an int and returns an int
 *  != 0 if the input is valid.  For instance, 'isdigit()'.
 */
void check_text_input(GtkEditable *editable,
                      gchar       *new_text,
                      gint         new_text_length,
                      gint        *position,
                      gpointer     user_data)
{
    int i;
    int (*check_func)(int) = NULL;

    if(user_data == NULL)
    {
        return;
    }

    g_signal_handlers_block_by_func(editable,
                                    (gpointer)check_text_input, user_data);
    check_func = (int (*)(int))user_data;

    for(i = 0; i < new_text_length; i++)
    {
        if(!check_func(new_text[i]))
            goto invalid_input;
    }

    gtk_editable_insert_text(editable, new_text, new_text_length, position);

invalid_input:
    g_signal_handlers_unblock_by_func(editable,
                                      (gpointer)check_text_input, user_data);
    g_signal_stop_emission_by_name(editable, "insert-text");
}

