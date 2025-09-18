/* hospital_management_system.c
   GTK3 single-file Hospital Management System (improved)
   - Splash screen (progress)
   - Maximized main window
   - Patients (Add / Edit / Delete / Search / Export CSV / Clear)
   - AI Assistant (keyword-based)
   - Persistent storage: patients.txt (name|age|gender|timestamp)
   Compile:
     gcc hospital_management_system.c -o hospital_mgmt $(pkg-config --cflags --libs gtk+-3.0)
*/

#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum { COL_NAME = 0, COL_AGE, COL_GENDER, COL_DATE, NUM_COLS };

/* Global widgets/models */
static GtkWidget *g_entry_name;
static GtkWidget *g_entry_age;
static GtkWidget *g_entry_gender;
static GtkWidget *g_status_patients;
static GtkWidget *g_status_ai;
static GtkListStore *g_store;
static GtkTreeModel *g_filter;
static GtkWidget *g_search_entry;
static GtkWidget *g_treeview;

/* ---------- Utilities ---------- */

static void current_timestamp(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}

/* Write liststore contents to patients.txt (overwrite) */
static void write_store_to_file(GtkListStore *store) {
    FILE *f = fopen("patients.txt", "w");
    if (!f) return;
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gchar *name = NULL, *age = NULL, *gender = NULL, *date = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, COL_DATE, &date, -1);
        fprintf(f, "%s|%s|%s|%s\n", name ? name : "", age ? age : "", gender ? gender : "", date ? date : "");
        if (name) g_free(name);
        if (age) g_free(age);
        if (gender) g_free(gender);
        if (date) g_free(date);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    fclose(f);
}

/* Load patients.txt into liststore (clears first) */
static void load_file_to_store(GtkListStore *store) {
    gtk_list_store_clear(store);
    FILE *f = fopen("patients.txt", "r");
    if (!f) return;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = strchr(line, '\n'); if (p) *p = '\0';
        /* parse name|age|gender|date */
        char copy[1024];
        strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';
        char *name = strtok(copy, "|");
        char *age = strtok(NULL, "|");
        char *gender = strtok(NULL, "|");
        char *date = strtok(NULL, "|");
        if (name && age && gender && date) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               COL_NAME, name,
                               COL_AGE,  age,
                               COL_GENDER, gender,
                               COL_DATE, date,
                               -1);
        }
    }
    fclose(f);
}

/* Case-insensitive exact name lookup in the store */
static gboolean store_has_name(GtkListStore *store, const char *name_to_find) {
    if (!name_to_find || !name_to_find[0]) return FALSE;
    gchar *target = g_utf8_strdown(name_to_find, -1);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gchar *name = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_NAME, &name, -1);
        if (name) {
            gchar *nlow = g_utf8_strdown(name, -1);
            if (g_strcmp0(nlow, target) == 0) {
                g_free(name); g_free(nlow); g_free(target);
                return TRUE;
            }
            g_free(name); g_free(nlow);
        }
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    g_free(target);
    return FALSE;
}

/* Export CSV */
static void export_csv(GtkWidget *btn, gpointer user_data) {
    const char *outname = "patients_export.csv";
    FILE *f = fopen(outname, "w");
    if (!f) {
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Failed to export CSV.");
        return;
    }
    fprintf(f, "Name,Age,Gender,Added\n");
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(g_store), &iter);
    while (valid) {
        gchar *name = NULL, *age = NULL, *gender = NULL, *date = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(g_store), &iter, COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, COL_DATE, &date, -1);
        /* simple CSV escaping by wrapping quotes if needed */
        fprintf(f, "\"%s\",\"%s\",\"%s\",\"%s\"\n", name ? name : "", age ? age : "", gender ? gender : "", date ? date : "");
        if (name) g_free(name);
        if (age) g_free(age);
        if (gender) g_free(gender);
        if (date) g_free(date);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(g_store), &iter);
    }
    fclose(f);
    gtk_label_set_text(GTK_LABEL(g_status_patients), "Exported to patients_export.csv");
}

/* ---------- Filter callback for search ---------- */
static gboolean filter_visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data) {
    gchar *name = NULL;
    gtk_tree_model_get(model, iter, COL_NAME, &name, -1);
    const gchar *q = gtk_entry_get_text(GTK_ENTRY(g_search_entry));
    gboolean ok = TRUE;
    if (q && q[0] != '\0') {
        gchar *lower_name = g_utf8_strdown(name ? name : "", -1);
        gchar *lower_q = g_utf8_strdown(q, -1);
        ok = (strstr(lower_name, lower_q) != NULL);
        g_free(lower_name); g_free(lower_q);
    }
    if (name) g_free(name);
    return ok;
}

/* Refresh view (reload file and refilter) */
static void refresh_view(void) {
    load_file_to_store(g_store);
    if (GTK_IS_TREE_MODEL_FILTER(g_filter))
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(g_filter));
}

/* ---------- Dialogs & editing ---------- */

/* Helper: get selection's store iter (maps filter->child) */
static gboolean get_selected_store_iter(GtkTreeIter *store_iter_out) {
    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_treeview));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(g_treeview));
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return FALSE;
    if (GTK_IS_TREE_MODEL_FILTER(model)) {
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), store_iter_out, &iter);
    } else {
        *store_iter_out = iter;
    }
    return TRUE;
}

/* Show edit dialog for a particular store iter */
static void show_edit_dialog(GtkWindow *parent, GtkTreeIter *store_iter) {
    gchar *name = NULL, *age = NULL, *gender = NULL, *date = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(g_store), store_iter, COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, COL_DATE, &date, -1);

    GtkWidget *dlg = gtk_dialog_new_with_buttons("Edit patient",
                    parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                    "_Save", GTK_RESPONSE_ACCEPT, "_Cancel", GTK_RESPONSE_REJECT, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8); gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *e_name = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(e_name), name ? name : "");
    GtkWidget *e_age = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(e_age), age ? age : "");
    GtkWidget *e_gender = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(e_gender), gender ? gender : "");

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e_name, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Age:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e_age, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Gender:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e_gender, 1, 2, 1, 1);

    gtk_widget_show_all(dlg);
    gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    if (resp == GTK_RESPONSE_ACCEPT) {
        const gchar *nn = gtk_entry_get_text(GTK_ENTRY(e_name));
        const gchar *na = gtk_entry_get_text(GTK_ENTRY(e_age));
        const gchar *ng = gtk_entry_get_text(GTK_ENTRY(e_gender));
        /* update store and file */
        gtk_list_store_set(g_store, store_iter, COL_NAME, nn, COL_AGE, na, COL_GENDER, ng, -1);
        /* ensure file sync */
        write_store_to_file(g_store);
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Patient updated.");
        /* refilter */
        if (GTK_IS_TREE_MODEL_FILTER(g_filter)) gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(g_filter));
    }
    gtk_widget_destroy(dlg);
    if (name) g_free(name); if (age) g_free(age); if (gender) g_free(gender); if (date) g_free(date);
}

/* ---------- Callbacks for Buttons ---------- */

static void on_add_clicked(GtkButton *btn, gpointer user_data) {
    const gchar *name = gtk_entry_get_text(GTK_ENTRY(g_entry_name));
    const gchar *age = gtk_entry_get_text(GTK_ENTRY(g_entry_age));
    const gchar *gender = gtk_entry_get_text(GTK_ENTRY(g_entry_gender));
    if (!name || !name[0] || !age || !age[0] || !gender || !gender[0]) {
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Please fill all fields.");
        return;
    }
    if (store_has_name(g_store, name)) {
        /* show existing details in status */
        GtkTreeIter iter;
        gboolean found = FALSE;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(g_store), &iter);
        while (valid) {
            gchar *n = NULL, *a = NULL, *g = NULL, *d = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(g_store), &iter, COL_NAME, &n, COL_AGE, &a, COL_GENDER, &g, COL_DATE, &d, -1);
            if (n) {
                gchar *nl = g_utf8_strdown(n, -1);
                gchar *ql = g_utf8_strdown(name, -1);
                if (g_strcmp0(nl, ql) == 0) {
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Patient exists: %s | Age: %s | Gender: %s | Added: %s", n, a ? a : "-", g ? g : "-", d ? d : "-");
                    gtk_label_set_text(GTK_LABEL(g_status_patients), buf);
                    found = TRUE;
                    g_free(nl); g_free(ql);
                    if (n) g_free(n); if (a) g_free(a); if (g) g_free(g); if (d) g_free(d);
                    break;
                }
                g_free(nl); g_free(ql);
            }
            if (n) g_free(n); if (a) g_free(a); if (g) g_free(g); if (d) g_free(d);
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(g_store), &iter);
        }
        if (!found) gtk_label_set_text(GTK_LABEL(g_status_patients), "Patient already exists.");
        return;
    }
    /* add */
    char ts[64]; current_timestamp(ts, sizeof(ts));
    GtkTreeIter iter;
    gtk_list_store_append(g_store, &iter);
    gtk_list_store_set(g_store, &iter, COL_NAME, name, COL_AGE, age, COL_GENDER, gender, COL_DATE, ts, -1);
    write_store_to_file(g_store);
    gtk_label_set_text(GTK_LABEL(g_status_patients), "Patient added.");
    gtk_entry_set_text(GTK_ENTRY(g_entry_name), "");
    gtk_entry_set_text(GTK_ENTRY(g_entry_age), "");
    gtk_entry_set_text(GTK_ENTRY(g_entry_gender), "");
    if (GTK_IS_TREE_MODEL_FILTER(g_filter)) gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(g_filter));
}

static void on_edit_clicked(GtkButton *btn, gpointer user_data) {
    GtkTreeIter store_iter;
    if (!get_selected_store_iter(&store_iter)) {
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Select a patient to edit.");
        return;
    }
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn)));
    show_edit_dialog(parent, &store_iter);
}

static void on_delete_clicked(GtkButton *btn, gpointer user_data) {
    GtkTreeIter store_iter;
    if (!get_selected_store_iter(&store_iter)) {
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Select a patient to delete.");
        return;
    }
    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn))),
                                                 GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                                                 "Delete selected patient?");
    gint r = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (r == GTK_RESPONSE_YES) {
        gtk_list_store_remove(g_store, &store_iter);
        write_store_to_file(g_store);
        gtk_label_set_text(GTK_LABEL(g_status_patients), "Patient deleted.");
    }
}

/* Double click to edit -> connect to "row-activated" */
static void on_row_activated(GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data) {
    GtkTreeModel *model;
    GtkTreeIter filter_iter;
    model = gtk_tree_view_get_model(tree);
    if (!gtk_tree_model_get_iter(model, &filter_iter, path)) return;
    GtkTreeIter store_iter;
    if (GTK_IS_TREE_MODEL_FILTER(model)) {
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &store_iter, &filter_iter);
    } else {
        store_iter = filter_iter;
    }
    GtkWindow *parent = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(tree)));
    show_edit_dialog(parent, &store_iter);
}

/* Refresh and search handlers */
static void on_refresh_clicked(GtkWidget *w, gpointer d) { refresh_view(); }
static void on_search_changed(GtkEditable *e, gpointer d) { gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(g_filter)); }
static void on_clear_all_clicked(GtkWidget *w, gpointer d) { FILE *f = fopen("patients.txt","w"); if (f) fclose(f); gtk_list_store_clear(g_store); gtk_label_set_text(GTK_LABEL(g_status_patients), "All data cleared."); }

/* ---------- AI Assistant ---------- */

static const struct { const char *keyword; const char *department; } ai_map[] = {
    {"chest","Cardiology"},{"heart","Cardiology"},{"fever","General Medicine"},{"cough","Pulmonology / General Medicine"},
    {"pain","General Medicine"},{"bone","Orthopedics"},{"fracture","Orthopedics"},{"child","Pediatrics"},
    {"preg","Obstetrics & Gynaecology"},{"skin","Dermatology"},{"headache","Neurology"},{"dizziness","Neurology"},
    {"eye","Ophthalmology"},{"ear","ENT"},{"nose","ENT"},{NULL,NULL}
};

static const char *ai_suggest(const char *symptoms) {
    if (!symptoms || *symptoms == '\0') return "Please enter symptoms.";
    gchar *low = g_utf8_strdown(symptoms, -1);
    for (int i = 0; ai_map[i].keyword; ++i) {
        if (strstr(low, ai_map[i].keyword)) { g_free(low); return ai_map[i].department; }
    }
    g_free(low);
    return "General Medicine (no strong match)";
}
static void on_ai_ask_clicked(GtkButton *btn, gpointer user_data) {
    GtkEntry *entry = GTK_ENTRY(user_data);
    const gchar *sym = gtk_entry_get_text(entry);
    const char *dept = ai_suggest(sym);
    char buf[256]; snprintf(buf, sizeof(buf), "Suggested department: %s", dept);
    gtk_label_set_text(GTK_LABEL(g_status_ai), buf);
}

/* ---------- UI Building ---------- */

static void apply_css(void) {
    GtkCssProvider *prov = gtk_css_provider_new();
    const gchar *css =
        "window { background: #f6fbff; }"
        "headerbar { background-image: linear-gradient(90deg, #0f172a, #2563eb); color: white; }"
        "button { border-radius: 6px; padding: 6px 10px; }"
        "entry { border-radius: 4px; padding: 6px; }"
        "treeview { background: white; }"
        "label { color: #0b1320; }";
    gtk_css_provider_load_from_data(prov, css, -1, NULL);
    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(prov), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(prov);
}

static void create_main_window(void) {
    /* Build main window and all UI, using globals */
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);
    gtk_window_set_title(GTK_WINDOW(window), "Hospital Management System");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_window_maximize(GTK_WINDOW(window));

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Hospital Management System");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    /* Main layout: vertical box with notebook */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(main_box), notebook, TRUE, TRUE, 8);

    /* --- Patients Tab --- */
    GtkWidget *patients_page = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(patients_page), 12);

    /* Left: form and controls */
    GtkWidget *left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(left, 360, -1);
    gtk_box_pack_start(GTK_BOX(patients_page), left, FALSE, FALSE, 0);

    GtkWidget *form = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(form), 8);
    gtk_grid_set_column_spacing(GTK_GRID(form), 8);
    gtk_box_pack_start(GTK_BOX(left), form, FALSE, FALSE, 0);

    gtk_grid_attach(GTK_GRID(form), gtk_label_new("Name:"), 0, 0, 1, 1);
    g_entry_name = gtk_entry_new(); gtk_grid_attach(GTK_GRID(form), g_entry_name, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(form), gtk_label_new("Age:"), 0, 1, 1, 1);
    g_entry_age = gtk_entry_new(); gtk_grid_attach(GTK_GRID(form), g_entry_age, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(form), gtk_label_new("Gender:"), 0, 2, 1, 1);
    g_entry_gender = gtk_entry_new(); gtk_grid_attach(GTK_GRID(form), g_entry_gender, 1, 2, 1, 1);

    GtkWidget *btn_add = gtk_button_new_with_label("Add Patient");
    gtk_box_pack_start(GTK_BOX(left), btn_add, FALSE, FALSE, 0);
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_clicked), NULL);

    /* Buttons row: Edit / Delete / Export / Clear / Refresh */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(left), btn_row, FALSE, FALSE, 0);

    GtkWidget *btn_edit = gtk_button_new_with_label("Edit Selected");
    GtkWidget *btn_delete = gtk_button_new_with_label("Delete Selected");
    GtkWidget *btn_export = gtk_button_new_with_label("Export CSV");
    GtkWidget *btn_clear = gtk_button_new_with_label("Clear All");
    GtkWidget *btn_refresh = gtk_button_new_with_label("Refresh");

    gtk_box_pack_start(GTK_BOX(btn_row), btn_edit, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_row), btn_delete, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_row), btn_export, TRUE, TRUE, 0);

    /* second small row */
    GtkWidget *btn_row2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(left), btn_row2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(btn_row2), btn_refresh, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(btn_row2), btn_clear, TRUE, TRUE, 0);

    g_status_patients = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(left), g_status_patients, FALSE, FALSE, 6);

    /* Right: search + treeview */
    GtkWidget *right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_box_pack_start(GTK_BOX(patients_page), right, TRUE, TRUE, 0);

    g_search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_search_entry), "Search by name...");
    gtk_box_pack_start(GTK_BOX(right), g_search_entry, FALSE, FALSE, 0);

    /* model and filter */
    g_store = GTK_LIST_STORE(gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING));
    load_file_to_store(g_store);
    g_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(g_store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(g_filter), filter_visible_func, NULL, NULL);

    g_treeview = gtk_tree_view_new_with_model(g_filter);
    gtk_widget_set_vexpand(g_treeview, TRUE);
    gtk_widget_set_vexpand(g_treeview, TRUE);

    GtkCellRenderer *r;
    GtkTreeViewColumn *c;
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Name", r, "text", COL_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_treeview), c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Age", r, "text", COL_AGE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_treeview), c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Gender", r, "text", COL_GENDER, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_treeview), c);
    r = gtk_cell_renderer_text_new();
    c = gtk_tree_view_column_new_with_attributes("Added", r, "text", COL_DATE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_treeview), c);

    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled), g_treeview);
    gtk_box_pack_start(GTK_BOX(right), scrolled, TRUE, TRUE, 0);

    /* wire signals */
    g_signal_connect(g_search_entry, "changed", G_CALLBACK(on_search_changed), NULL);
    g_signal_connect(btn_edit, "clicked", G_CALLBACK(on_edit_clicked), NULL);
    g_signal_connect(btn_delete, "clicked", G_CALLBACK(on_delete_clicked), NULL);
    g_signal_connect(btn_export, "clicked", G_CALLBACK(export_csv), NULL);
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_all_clicked), NULL);
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    g_signal_connect(g_treeview, "row-activated", G_CALLBACK(on_row_activated), NULL);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), patients_page, gtk_label_new("Patients"));

    /* --- AI Tab --- */
    GtkWidget *ai_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(ai_box), 12);
    GtkWidget *ai_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(ai_label), "<span size='large' weight='bold'>Smart Assistant</span>\nEnter symptoms and get a suggested department.");
    gtk_box_pack_start(GTK_BOX(ai_box), ai_label, FALSE, FALSE, 0);

    GtkWidget *ai_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ai_entry), "e.g. chest pain, fever, cough...");
    gtk_box_pack_start(GTK_BOX(ai_box), ai_entry, FALSE, FALSE, 0);

    GtkWidget *ai_btn = gtk_button_new_with_label("Ask Assistant");
    gtk_box_pack_start(GTK_BOX(ai_box), ai_btn, FALSE, FALSE, 0);
    g_status_ai = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(ai_box), g_status_ai, FALSE, FALSE, 0);
    g_signal_connect(ai_btn, "clicked", G_CALLBACK(on_ai_ask_clicked), ai_entry);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ai_box, gtk_label_new("AI Assistant"));

    /* Pack and show */
    gtk_widget_show_all(window);
}

/* ---------- Splash progress handling ---------- */
typedef struct {
    GtkWidget *splash;
    GtkWidget *progress;
    int ticks;
    int max_ticks;
} SplashData;

static gboolean splash_tick(gpointer data) {
    SplashData *sd = (SplashData*)data;
    sd->ticks++;
    double frac = (double)sd->ticks / (double)sd->max_ticks;
    if (frac > 1.0) frac = 1.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(sd->progress), frac);

    if (sd->ticks >= sd->max_ticks) {
        /* create main window and destroy splash */
        create_main_window();
        if (GTK_IS_WIDGET(sd->splash)) gtk_widget_destroy(sd->splash);
        g_free(sd);
        return FALSE; /* stop timeout */
    }
    return TRUE; /* continue */
}

/* ---------- Main ---------- */

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    apply_css();

    /* Create splash */
    GtkWidget *splash = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(splash), FALSE);
    gtk_window_set_keep_above(GTK_WINDOW(splash), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(splash), 420, 140);
    gtk_window_set_position(GTK_WINDOW(splash), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(splash), 12);

    GtkWidget *v = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(splash), v);
    GtkWidget *lbl = gtk_label_new("Starting Hospital Management System...");
    gtk_box_pack_start(GTK_BOX(v), lbl, TRUE, TRUE, 0);
    GtkWidget *progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress), FALSE);
    gtk_box_pack_start(GTK_BOX(v), progress, FALSE, FALSE, 0);

    gtk_widget_show_all(splash);

    /* Splash data */
    SplashData *sd = g_new0(SplashData, 1);
    sd->splash = splash; sd->progress = progress; sd->ticks = 0; sd->max_ticks = 10; /* 10 ticks */
    g_timeout_add(180, splash_tick, sd); /* ~1.8s total */

    gtk_main();
    return 0;
}
