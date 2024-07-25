#include <gtk/gtk.h>

#define WINDOW_HEIGHT 400
#define WINDOW_WIDTH 350

#define RENDER_TYPE_FAIL 0
#define RENDER_TYPE_IMAGE 1
#define RENDER_TYPE_TEXT 2
#define RENDER_TYPE_UNKNOWN 3

const gchar *historydir = NULL;

GtkWidget *render_listing(const gchar *listing);

void selectaction(GtkListBoxRow *row) {
    const GtkListBox *listbox = gtk_widget_get_parent(row);
    const char *listing = g_object_get_data(row, "listing");
    printf("%s\n", listing);
    gtk_main_quit();
}

void deleteaction(GtkListBoxRow *row) {
    const GtkListBox *listbox = gtk_widget_get_parent(row);
    const char *listing = g_object_get_data(row, "listing");

    gchar *listingdir = g_build_filename(historydir, listing, NULL);
    if (!g_file_trash(g_file_new_for_path(listingdir), NULL, NULL)) {
        system(g_strconcat("/bin/rm -r ", listingdir, NULL));
    }
    gtk_widget_destroy(row);
}

void pinaction(GtkListBoxRow *row) {
    GtkListBox *listbox = gtk_widget_get_parent(row);
    const char *listing = g_object_get_data(row, "listing");
    gchar *listingdir = g_build_filename(historydir, listing, NULL);
    gchar *negative = g_strdup_printf("%ld", -strtol(listing, NULL, 10));
    gchar *negative_listingdir = g_build_filename(historydir, negative, NULL);
    if (rename(listingdir, negative_listingdir) != 0) {
        fprintf(stderr, "Failed to pin/unpin item\n");
        return;
    }

    gtk_widget_destroy(row);
    GtkListBoxRow *updated_row = render_listing(negative);
    gtk_list_box_insert(listbox, updated_row, -1);
    gtk_list_box_invalidate_sort(listbox);
    gtk_widget_show_all(listbox);
}


gboolean onclick(GtkEventBox *eventbox, GdkEventButton *event, gpointer unused) {
    GtkListBoxRow *row = gtk_widget_get_parent(eventbox);
    switch (event->button) {
        case 1:
            selectaction(row);
            return TRUE;
        case 2:
            deleteaction(row);
            return TRUE;
        case 3:
            pinaction(row);
            return TRUE;
    }
}

int what_to_render(const gchar *listing, gchar **chosen_dir) {
    gchar *listing_dir = g_build_filename(historydir, listing, NULL);
    GDir *dir = g_dir_open(listing_dir, 0, NULL);
    if (!dir) {
        fprintf(stderr, "Failed to open listing %s\n", listing);
        return RENDER_TYPE_FAIL;
    }

    gchar *first_text_entry = NULL;
    const gchar *entry = NULL;
    while ((entry = g_dir_read_name(dir))) {
        if (!entry) {
            fprintf(stderr, "Error reading a variant of listing %s\n", listing);
            continue;
        }
        if (g_str_equal(entry, "text\\plain")) {
            *chosen_dir = g_build_filename(listing_dir, entry, NULL);
            return RENDER_TYPE_TEXT;
        }
        if (g_str_equal(entry, "image\\jpeg") || g_str_equal(entry, "image\\png") || g_str_equal(entry, "image\\svg+xml")) {
            *chosen_dir = g_build_filename(listing_dir, entry, NULL);
            return RENDER_TYPE_IMAGE;
        }
        if (!first_text_entry && g_str_has_prefix(entry, "text\\")) {
            first_text_entry = g_build_filename(listing_dir, entry, NULL);
        }
    }

    if (first_text_entry) {
        *chosen_dir = first_text_entry;
        return RENDER_TYPE_TEXT;
    }

    *chosen_dir = NULL;
    return RENDER_TYPE_UNKNOWN;
}

GtkWidget *render_listing(const gchar *listing) {
    GtkWidget *widget = NULL;
    gchar *chosen_dir = NULL;
    switch (what_to_render(listing, &chosen_dir)) {
        case RENDER_TYPE_TEXT: {
            gchar *data_file = g_build_filename(chosen_dir, "data", NULL);
            gchar *text = NULL;
            if (!g_file_get_contents(data_file, &text, NULL, NULL)) {
                fprintf(stderr, "Failed to read text data from %s\n", data_file);
                return NULL;
            }
            widget = gtk_label_new(g_utf8_substring(text, 0, MIN(strlen(text), 100)));  // dont ask
            g_free(text);
            gtk_label_set_ellipsize(widget, PANGO_ELLIPSIZE_END);
            break;
        }
        case RENDER_TYPE_IMAGE: {
            gchar *data_file = g_build_filename(chosen_dir, "data", NULL);
            GdkPixbuf *buffer = gdk_pixbuf_new_from_file_at_size(data_file, 400, 200, NULL);
            if (!buffer) {
                fprintf(stderr, "Failed to read image data from %s\n", data_file);
                return NULL;
            }
            widget = gtk_image_new_from_pixbuf(buffer);
            break;
        }
        case RENDER_TYPE_UNKNOWN:
            widget = gtk_label_new("(blob)");
            break;
        case RENDER_TYPE_FAIL:
        default:
            return NULL;
    }

    gchar *index_file = g_build_filename(chosen_dir, "index", NULL);
    gchar *index_data = NULL;
    if (!g_file_get_contents(index_file, &index_data, NULL, NULL)) {
        fprintf(stderr, "Failed to read index data from %s\n", index_file);
        return NULL;
    }
    GtkOverlay *overlay = gtk_overlay_new();
    gtk_container_add(overlay, widget);
    if (listing[0] == '-') {
        GtkImage *pinned_icon = gtk_image_new_from_icon_name("pin", GTK_ICON_SIZE_DND);
        gtk_widget_set_halign(pinned_icon, GTK_ALIGN_END);
        gtk_widget_set_valign(pinned_icon, GTK_ALIGN_START);
        gtk_overlay_add_overlay(overlay, pinned_icon);
    }

    GtkEventBox *eventbox = gtk_event_box_new();
    g_signal_connect(eventbox, "button-release-event", onclick, NULL);
    gtk_container_add(eventbox, overlay);

    GtkListBoxRow *row = gtk_list_box_row_new();
    gtk_container_add(row, eventbox);
    g_object_set_data(row, "listing", g_strdup(listing));
    g_object_set_data(row, "index", index_data);
    gtk_widget_set_tooltip_text(row, "Left click to copy, middle click to delete, right click to pin");

    return row;
}

gboolean onescape(GtkWidget *widget, GdkEventKey *event, gpointer unused) {
    if (event->keyval == GDK_KEY_Escape) {
        gtk_main_quit();
        return TRUE;
    }
    return FALSE;
}

int sorter(GtkListBoxRow *one, GtkListBoxRow *two, gpointer unused) {
    return strtoul(g_object_get_data(one, "listing"), NULL, 10) < strtoul(g_object_get_data(two, "listing"), NULL, 10);
}

int clamp(gint min, gint val, gint max) {
    if (val > max) return max;
    if (val < min) return min;
    return val;
}

gboolean triggerlistupdate(GtkSearchEntry *searchbox, GtkListBox *listbox) {
    gtk_list_box_invalidate_filter(listbox);
    return TRUE;
}

gboolean filterpredicate(GtkListBoxRow *row, GtkSearchEntry *searchbox) {
    const gchar *searchtext = gtk_entry_get_text(searchbox);
    const gchar *indexdata = g_object_get_data(row, "index");
    return g_str_match_string(searchtext, indexdata, TRUE);
}

int main() {
    gtk_init(NULL, NULL);

    GtkListBox *listbox = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(listbox, FALSE);
    gtk_list_box_set_sort_func(listbox, sorter, NULL, NULL);

    const gchar *default_base_dir = g_build_filename(getenv("HOME"), ".coffeepesto", NULL);
    const gchar *provided_dir = getenv("COFFEE_PESTO_DIR");
    const gchar *dir_to_use = provided_dir ? provided_dir : default_base_dir;
    historydir = g_build_filename(dir_to_use, "h", NULL);
    GDir *fd = g_dir_open(historydir, 0, NULL);
    if (!fd) {
        fprintf(stderr, "App directory %s doesn't exist\n", historydir);
        return 1;
    }

    const gchar *listing;
    GtkListBoxRow *row;
    while ((listing = g_dir_read_name(fd))) {
        if (!(row = render_listing(listing))) {
            fprintf(stderr, "Failed to render listing %s. Skipping\n", listing);
            continue;
        }
        gtk_list_box_insert(listbox, row, -1);
    }

    GtkScrolledWindow *scrollingcontainer = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(scrollingcontainer, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(scrollingcontainer, listbox);

    GtkSearchEntry *searchbox = gtk_search_entry_new();
    g_signal_connect(searchbox, "changed", triggerlistupdate, listbox);
    gtk_list_box_set_filter_func(listbox, filterpredicate, searchbox, NULL);

    GtkHeaderBar *header = gtk_header_bar_new();
    gtk_header_bar_set_title(header, "CoffeePesto");

    GtkBox *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(vbox, header, FALSE, FALSE, 0);
    gtk_box_pack_start(vbox, searchbox, FALSE, FALSE, 0);
    gtk_box_pack_start(vbox, scrollingcontainer, TRUE, TRUE, 0);

    GtkWindow *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(window, "CoffeePesto");
    gtk_window_set_decorated(window, FALSE);
    gtk_window_set_default_size(window, WINDOW_WIDTH, WINDOW_HEIGHT);
    g_signal_connect(window, "focus-out-event", gtk_main_quit, NULL);
    g_signal_connect(window, "key-press-event", onescape, NULL);
    gtk_container_add(window, vbox);
    gtk_widget_show_all(window);

    GdkScreen *screen = gtk_window_get_screen(window);
    GdkDisplay *display = gtk_widget_get_display(window);
    GdkMonitor *monitor = gdk_display_get_monitor_at_window(display, gtk_widget_get_window(window));
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    gint screenwidth = geometry.width;
    gint screenheight = geometry.height;
    gint mouseX = 0, mouseY = 0;
    GdkDevice *device = gdk_seat_get_pointer(gdk_display_get_default_seat(display));
    gdk_device_get_position(device, &screen, &mouseX, &mouseY);
    gint desiredX = clamp(100, mouseX, screenwidth - WINDOW_WIDTH - 100);  // 100 is leeway for taskbars n stuff
    gint desiredY = clamp(100, mouseY, screenheight - WINDOW_HEIGHT - 100);
    gtk_window_move(window, desiredX, desiredY);

    GtkCssProvider *cssprovider = gtk_css_provider_new();
    const gchar *css = "row { border-bottom: 0.01em solid @secondary_color; } list { padding: .4em; }";
    gtk_css_provider_load_from_data(cssprovider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gtk_window_get_screen(window), cssprovider,
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    gtk_main();
    return 0;
}
