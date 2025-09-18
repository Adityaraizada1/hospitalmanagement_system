#include <gtk/gtk.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h> // For g_usleep

// --- File Paths ---
#define PATIENTS_FILE "patients.txt"
// CSS is now embedded, so CSS_FILE is no longer needed.

// --- Structs and Enums ---

// For the login dialog
typedef struct {
    GtkWidget *window;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    gboolean success;
} LoginData;

// For the patients tab
enum {
    COL_NAME = 0,
    COL_AGE,
    COL_GENDER,
    COL_TIMESTAMP,
    NUM_COLS
};

typedef struct {
    GtkListStore *store;
    GtkTreeModel *filter_model;
    GtkWidget *tree_view;
    GtkWidget *search_entry;
} PatientWidgets;

// For the AI Assistant tab
typedef struct {
    GtkWidget *symptom_entry;
    GtkWidget *result_label;
} AIAssistantWidgets;


// --- Function Prototypes ---

// Main Application
static void create_main_window();
static void load_css();

// Utility Functions
char* get_current_timestamp();
void show_message(GtkWindow *parent, GtkMessageType type, const char *title, const char *message);

// Login Window
gboolean show_login_window(GtkWindow *parent);
static void on_login_clicked(GtkButton *button, gpointer user_data);

// Patients Tab
GtkWidget* create_patients_tab();
static void load_patients(GtkListStore *store);
// Modified to accept a parent window for its message dialogs
static void save_patients(GtkListStore *store, GtkWindow *parent_window);

// AI Assistant Tab
GtkWidget* create_ai_assistant_tab();


// --- Utility Function Implementations ---

// Gets the current timestamp as a formatted string
char* get_current_timestamp() {
    time_t rawtime;
    struct tm *timeinfo;
    static char buffer[20]; // Static buffer to be returned
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", timeinfo);
    return buffer;
}

// Shows a simple information or error message dialog
void show_message(GtkWindow *parent, GtkMessageType type, const char *title, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                               type,
                                               GTK_BUTTONS_OK,
                                               "%s",
                                               message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// --- Login Window Implementations ---

// Callback function for the login button
static void on_login_clicked(GtkButton *button, gpointer user_data) {
    LoginData *data = (LoginData *)user_data;
    const char *username = gtk_entry_get_text(GTK_ENTRY(data->username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(data->password_entry));

    // Hardcoded credentials
    if (strcmp(username, "admin") == 0 && strcmp(password, "password") == 0) {
        data->success = TRUE;
        gtk_widget_destroy(data->window);
    } else {
        show_message(GTK_WINDOW(data->window), GTK_MESSAGE_ERROR, "Login Failed", "Invalid username or password.");
        gtk_entry_set_text(GTK_ENTRY(data->password_entry), ""); // Clear password field
    }
}

// Main function to create and run the login dialog
gboolean show_login_window(GtkWindow *parent) {
    LoginData data = {0};
    data.success = FALSE;

    data.window = gtk_dialog_new_with_buttons("Staff Login", parent,
                                              GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                              NULL, NULL);
    gtk_window_set_position(GTK_WINDOW(data.window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(data.window), 10);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(data.window));
    gtk_box_set_spacing(GTK_BOX(content_area), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    GtkWidget *user_label = gtk_label_new("Username:");
    gtk_grid_attach(GTK_GRID(grid), user_label, 0, 0, 1, 1);
    data.username_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(data.username_entry), "admin");
    gtk_grid_attach(GTK_GRID(grid), data.username_entry, 1, 0, 1, 1);

    GtkWidget *pass_label = gtk_label_new("Password:");
    gtk_grid_attach(GTK_GRID(grid), pass_label, 0, 1, 1, 1);
    data.password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(data.password_entry), FALSE);
    gtk_entry_set_invisible_char(GTK_ENTRY(data.password_entry), '*');
    gtk_grid_attach(GTK_GRID(grid), data.password_entry, 1, 1, 1, 1);
    g_signal_connect_swapped(data.password_entry, "activate", G_CALLBACK(on_login_clicked), &data);

    GtkWidget *login_button = gtk_button_new_with_label("Login");
    gtk_widget_set_can_default(login_button, TRUE);
    gtk_widget_grab_default(login_button);
    gtk_widget_set_name(login_button, "loginButton"); // For CSS
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), &data);
    gtk_grid_attach(GTK_GRID(grid), login_button, 1, 2, 1, 1);

    gtk_widget_show_all(data.window);
    gtk_dialog_run(GTK_DIALOG(data.window)); // This blocks until the window is destroyed

    return data.success;
}

// --- AI Assistant Implementations ---

static void to_lower_case(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

static void on_suggest_department(GtkButton *button, gpointer user_data) {
    AIAssistantWidgets *widgets = (AIAssistantWidgets *)user_data;
    const char *symptoms_const = gtk_entry_get_text(GTK_ENTRY(widgets->symptom_entry));
    if (strlen(symptoms_const) == 0) {
        gtk_label_set_markup(GTK_LABEL(widgets->result_label), "<span size='large' weight='bold' foreground='#d35400'>Please enter symptoms first.</span>");
        return;
    }
    
    char *symptoms = g_strdup(symptoms_const);
    to_lower_case(symptoms);

    const char *suggestion = "General Medicine (More specific symptoms needed)";

    if (strstr(symptoms, "chest") || strstr(symptoms, "heart") || strstr(symptoms, "pressure")) suggestion = "Cardiology";
    else if (strstr(symptoms, "skin") || strstr(symptoms, "rash") || strstr(symptoms, "acne")) suggestion = "Dermatology";
    else if (strstr(symptoms, "headache") || strstr(symptoms, "dizzy") || strstr(symptoms, "numbness")) suggestion = "Neurology";
    else if (strstr(symptoms, "bone") || strstr(symptoms, "fracture") || strstr(symptoms, "joint")) suggestion = "Orthopedics";
    else if (strstr(symptoms, "child") || strstr(symptoms, "baby") || strstr(symptoms, "infant")) suggestion = "Pediatrics";
    else if (strstr(symptoms, "stomach") || strstr(symptoms, "digest") || strstr(symptoms, "acid")) suggestion = "Gastroenterology";

    char result_text[256];
    snprintf(result_text, sizeof(result_text), "<span size='large' weight='bold'>Suggested Department: %s</span>", suggestion);
    gtk_label_set_markup(GTK_LABEL(widgets->result_label), result_text);
    g_free(symptoms);
}

GtkWidget* create_ai_assistant_tab() {
    AIAssistantWidgets *widgets = g_slice_new(AIAssistantWidgets);
    GtkWidget *grid = gtk_grid_new();
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 20);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='xx-large' weight='bold'>AI Department Assistant</span>");
    gtk_grid_attach(GTK_GRID(grid), title, 0, 0, 2, 1);
    
    GtkWidget *instruct_label = gtk_label_new("Enter patient symptoms below (e.g., 'chest pain', 'headache'):");
    gtk_grid_attach(GTK_GRID(grid), instruct_label, 0, 1, 2, 1);

    widgets->symptom_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->symptom_entry), "Enter symptoms here...");
    gtk_widget_set_size_request(widgets->symptom_entry, 400, -1);
    gtk_grid_attach(GTK_GRID(grid), widgets->symptom_entry, 0, 2, 1, 1);

    GtkWidget *suggest_button = gtk_button_new_with_label("Suggest Department");
    gtk_widget_set_name(suggest_button, "suggestButton");
    gtk_grid_attach(GTK_GRID(grid), suggest_button, 1, 2, 1, 1);

    widgets->result_label = gtk_label_new("Suggested Department: N/A");
    gtk_widget_set_name(widgets->result_label, "resultLabel");
    gtk_grid_attach(GTK_GRID(grid), widgets->result_label, 0, 3, 2, 1);

    g_signal_connect(suggest_button, "clicked", G_CALLBACK(on_suggest_department), widgets);
    g_signal_connect_swapped(widgets->symptom_entry, "activate", G_CALLBACK(gtk_button_clicked), suggest_button);

    return grid;
}

// --- Patients Tab Implementations ---

static gboolean filter_patients(GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
static void on_search_changed(GtkSearchEntry *entry, PatientWidgets *widgets);
static void on_add_patient(GtkButton *button, PatientWidgets *widgets);
static void on_edit_patient(GtkButton *button, PatientWidgets *widgets);
static void on_delete_patient(GtkButton *button, PatientWidgets *widgets);
static void on_export_csv(GtkButton *button, PatientWidgets *widgets);

GtkWidget* create_patients_tab() {
    PatientWidgets *widgets = g_slice_new(PatientWidgets);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    widgets->search_entry = gtk_search_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), widgets->search_entry, FALSE, FALSE, 0);

    GtkWidget *add_button = gtk_button_new_with_label("Add");
    GtkWidget *edit_button = gtk_button_new_with_label("Edit");
    GtkWidget *delete_button = gtk_button_new_with_label("Delete");
    GtkWidget *export_button = gtk_button_new_with_label("Export to CSV");

    gtk_box_pack_end(GTK_BOX(hbox), export_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), delete_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), edit_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), add_button, FALSE, FALSE, 0);
    
    gtk_widget_set_name(add_button, "addButton");

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    widgets->tree_view = gtk_tree_view_new();
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(widgets->tree_view), GTK_TREE_VIEW_GRID_LINES_VERTICAL);
    const char *column_titles[] = {"Name", "Age", "Gender", "Added"};
    for (int i = 0; i < NUM_COLS; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
            column_titles[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_column_set_sort_column_id(column, i);
        gtk_tree_view_append_column(GTK_TREE_VIEW(widgets->tree_view), column);
    }

    widgets->store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);
    load_patients(widgets->store);

    // Sorting
    for(int i = 0; i < NUM_COLS; ++i) {
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(widgets->store), i, GTK_SORT_ASCENDING);
    }

    widgets->filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(widgets->store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(widgets->filter_model), filter_patients, widgets->search_entry, NULL);
    
    gtk_tree_view_set_model(GTK_TREE_VIEW(widgets->tree_view), widgets->filter_model);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), widgets->tree_view);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_patient), widgets);
    g_signal_connect(edit_button, "clicked", G_CALLBACK(on_edit_patient), widgets);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_patient), widgets);
    g_signal_connect(export_button, "clicked", G_CALLBACK(on_export_csv), widgets);
    g_signal_connect(widgets->search_entry, "search-changed", G_CALLBACK(on_search_changed), widgets);

    return vbox;
}

static void load_patients(GtkListStore *store) {
    FILE *file = fopen(PATIENTS_FILE, "r");
    if (!file) return;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;
        char *name = strtok(line, ",");
        char *age_str = strtok(NULL, ",");
        char *gender = strtok(NULL, ",");
        char *timestamp = strtok(NULL, ",");

        if (name && age_str && gender && timestamp) {
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, COL_NAME, name, COL_AGE, atoi(age_str), COL_GENDER, gender, COL_TIMESTAMP, timestamp, -1);
        }
    }
    fclose(file);
}

static void save_patients(GtkListStore *store, GtkWindow *parent_window) {
    FILE *file = fopen(PATIENTS_FILE, "w");
    if (!file) {
        show_message(parent_window, GTK_MESSAGE_ERROR, "Error", "Could not save patient data.");
        return;
    }
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
        do {
            char *name, *gender, *timestamp;
            guint age;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, COL_TIMESTAMP, &timestamp, -1);
            fprintf(file, "%s,%u,%s,%s\n", name, age, gender, timestamp);
            g_free(name); g_free(gender); g_free(timestamp);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
    }
    fclose(file);
}

static gboolean show_patient_dialog(GtkWindow *parent, const char* title, char **name, guint *age, char **gender) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons(title, parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, "_OK", GTK_RESPONSE_ACCEPT, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(content_area), grid);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    GtkWidget *name_entry = gtk_entry_new();
    if (*name) gtk_entry_set_text(GTK_ENTRY(name_entry), *name);
    gtk_grid_attach(GTK_GRID(grid), name_entry, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Age:"), 0, 1, 1, 1);
    GtkWidget *age_spin = gtk_spin_button_new_with_range(0, 150, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(age_spin), *age);
    gtk_grid_attach(GTK_GRID(grid), age_spin, 1, 1, 1, 1);
    
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Gender:"), 0, 2, 1, 1);
    GtkWidget *gender_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gender_combo), "Male");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gender_combo), "Female");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gender_combo), "Other");
    if (*gender) {
        if (strcmp(*gender, "Male") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(gender_combo), 0);
        else if (strcmp(*gender, "Female") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(gender_combo), 1);
        else gtk_combo_box_set_active(GTK_COMBO_BOX(gender_combo), 2);
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(gender_combo), 0);
    }
    gtk_grid_attach(GTK_GRID(grid), gender_combo, 1, 2, 1, 1);
    gtk_widget_show_all(dialog);

    gboolean result = FALSE;
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char* name_text = gtk_entry_get_text(GTK_ENTRY(name_entry));
        if (strlen(name_text) == 0) {
            show_message(GTK_WINDOW(dialog), GTK_MESSAGE_ERROR, "Input Error", "Name cannot be empty.");
        } else {
            g_free(*name); g_free(*gender);
            *name = g_strdup(name_text);
            *age = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(age_spin));
            *gender = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(gender_combo));
            result = TRUE;
        }
    }
    gtk_widget_destroy(dialog);
    return result;
}

static void on_add_patient(GtkButton *button, PatientWidgets *widgets) {
    char *name = NULL, *gender = NULL; guint age = 30;
    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    if (show_patient_dialog(parent_window, "Add New Patient", &name, &age, &gender)) {
        GtkTreeIter iter;
        gtk_list_store_append(widgets->store, &iter);
        gtk_list_store_set(widgets->store, &iter, COL_NAME, name, COL_AGE, age, COL_GENDER, gender, COL_TIMESTAMP, get_current_timestamp(), -1);
        save_patients(widgets->store, parent_window);
    }
    g_free(name); g_free(gender);
}

static void on_edit_patient(GtkButton *button, PatientWidgets *widgets) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->tree_view));
    GtkTreeIter iter, store_iter; GtkTreeModel *model;
    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        char *name = NULL, *gender = NULL; guint age = 0;
        gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &store_iter, &iter);
        gtk_tree_model_get(GTK_TREE_MODEL(widgets->store), &store_iter, COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, -1);
        if (show_patient_dialog(parent_window, "Edit Patient", &name, &age, &gender)) {
            gtk_list_store_set(widgets->store, &store_iter, COL_NAME, name, COL_AGE, age, COL_GENDER, gender, -1);
            save_patients(widgets->store, parent_window);
        }
        g_free(name); g_free(gender);
    } else {
        show_message(parent_window, GTK_MESSAGE_INFO, "No Selection", "Please select a patient to edit.");
    }
}

static void on_delete_patient(GtkButton *button, PatientWidgets *widgets) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widgets->tree_view));
    GtkTreeIter iter, store_iter; GtkTreeModel *model;
    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkWidget *dialog = gtk_message_dialog_new(parent_window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Are you sure you want to delete this patient?");
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_YES) {
            gtk_tree_model_filter_convert_iter_to_child_iter(GTK_TREE_MODEL_FILTER(model), &store_iter, &iter);
            gtk_list_store_remove(widgets->store, &store_iter);
            save_patients(widgets->store, parent_window);
        }
        gtk_widget_destroy(dialog);
    } else {
        show_message(parent_window, GTK_MESSAGE_INFO, "No Selection", "Please select a patient to delete.");
    }
}

static void on_export_csv(GtkButton *button, PatientWidgets *widgets) {
    GtkWindow *parent_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button)));
    FILE *file = fopen("patients_export.csv", "w");
    if (!file) {
        show_message(parent_window, GTK_MESSAGE_ERROR, "Export Error", "Could not create patients_export.csv");
        return;
    }
    fprintf(file, "Name,Age,Gender,Added\n");
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(widgets->store), &iter)) {
        do {
            char *name, *gender, *timestamp; guint age;
            gtk_tree_model_get(GTK_TREE_MODEL(widgets->store), &iter, COL_NAME, &name, COL_AGE, &age, COL_GENDER, &gender, COL_TIMESTAMP, &timestamp, -1);
            fprintf(file, "\"%s\",%u,\"%s\",\"%s\"\n", name, age, gender, timestamp);
            g_free(name); g_free(gender); g_free(timestamp);
        } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(widgets->store), &iter));
    }
    fclose(file);
    show_message(parent_window, GTK_MESSAGE_INFO, "Export Success", "Patient data exported to patients_export.csv");
}

static gboolean filter_patients(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    GtkSearchEntry *search_entry = GTK_SEARCH_ENTRY(data);
    const char *search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    if (search_text == NULL || *search_text == '\0') return TRUE;
    char *name;
    gtk_tree_model_get(model, iter, COL_NAME, &name, -1);
    gchar *search_lower = g_utf8_strdown(search_text, -1);
    gchar *name_lower = g_utf8_strdown(name, -1);
    gboolean found = (strstr(name_lower, search_lower) != NULL);
    g_free(name); g_free(search_lower); g_free(name_lower);
    return found;
}

static void on_search_changed(GtkSearchEntry *entry, PatientWidgets *widgets) {
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(widgets->filter_model));
}


// --- Main Application Implementations ---

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    load_css();

    // --- Splash Screen Phase ---
    GtkWidget *splash_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(splash_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(splash_window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(splash_window), 450, 120);
    gtk_widget_set_name(splash_window, "splashWindow");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(splash_window), vbox);

    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), "<span size='x-large' weight='bold'>Hospital Management System</span>");
    GtkWidget *progress_bar = gtk_progress_bar_new();
    gtk_widget_set_name(progress_bar, "splashProgress");


    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, TRUE, TRUE, 0);

    gtk_widget_show_all(splash_window);

    // Manually run the GTK event loop for the splash screen for ~2 seconds.
    // This keeps the UI responsive without blocking everything.
    for (int i = 0; i <= 100; i++) {
        // Process all pending GTK events.
        while (gtk_events_pending()) {
            gtk_main_iteration();
        }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), i / 100.0);
        g_usleep(20000); // Sleep for 20ms (20ms * 100 = 2 seconds)
    }

    gtk_widget_destroy(splash_window);

    // --- Login Phase ---
    // The show_login_window function has its own internal loop (gtk_dialog_run).
    // We pass NULL as the parent because the splash screen is already destroyed.
    if (show_login_window(NULL)) {
        // --- Main Application Phase ---
        create_main_window();
        gtk_main(); // Start the main application event loop.
    }

    return 0;
}


void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    // Embed a modern, professional CSS theme
    const char *css_data =
        "/* --- Global Settings --- */"
        "@define-color theme_bg_color #f6f8fa;"
        "@define-color theme_fg_color #24292e;"
        "@define-color theme_selected_bg_color #0366d6;"
        "@define-color theme_selected_fg_color #ffffff;"
        "@define-color borders #dfe2e5;"
        "@define-color green #28a745;"
        "@define-color green_hover #22863a;"
        "window { background-color: @theme_bg_color; color: @theme_fg_color; }"
        
        "/* --- Splash Screen --- */"
        "#splashWindow { background-image: linear-gradient(to bottom, #ffffff, #f0f2f4); border-radius: 10px; border: 1px solid @borders; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }"
        "#splashProgress progress { background-color: @green; border-radius: 5px; border: none; }"
        "#splashProgress trough { background-color: #e1e4e8; border-radius: 5px; border: none; }"
        
        "/* --- Header Bar --- */"
        "headerbar { background-color: @theme_bg_color; padding: 6px; border-bottom: 1px solid @borders; box-shadow: 0 1px 3px rgba(0,0,0,0.05); }"
        "headerbar .title { font-weight: 600; font-size: 1.1em; }"

        "/* --- Buttons --- */"
        "button { font-weight: 600; border-radius: 6px; padding: 8px 16px; background-image: none; background-color: #fafbfc; border: 1px solid rgba(27, 31, 35, 0.15); box-shadow: 0 1px 0 rgba(27, 31, 35, 0.04); transition: all 0.2s; }"
        "button:hover { background-color: #f3f4f6; border-color: rgba(27, 31, 35, 0.2); }"
        "button:active { background-color: #edeff2; box-shadow: inset 0 1px 0 rgba(225, 228, 232, 0.2); }"
        "#loginButton, #suggestButton, #addButton { background-color: @green; color: white; border: 1px solid rgba(27, 31, 35, 0.15); }"
        "#loginButton:hover, #suggestButton:hover, #addButton:hover { background-color: @green_hover; }"
        
        "/* --- Entries & Inputs --- */"
        "entry, spinbutton, searchbar { padding: 8px; border-radius: 6px; border: 1px solid #d1d5da; box-shadow: inset 0 1px 2px rgba(27,31,35,0.075); }"
        "entry:focus, spinbutton:focus, searchbar:focus { border-color: @theme_selected_bg_color; box-shadow: 0 0 0 3px rgba(3, 102, 214, 0.3); }"

        "/* --- TreeView (Patient List) --- */"
        "treeview { border-radius: 6px; border: 1px solid @borders; }"
        "treeview header button { font-weight: 600; padding: 10px; background-color: @theme_bg_color; border: none; border-bottom: 1px solid @borders; }"
        "treeview row:nth-child(even) { background-color: #fbfcfd; }"
        "treeview row:selected { background-color: @theme_selected_bg_color; color: @theme_selected_fg_color; }"
        
        "/* --- Notebook (Tabs) --- */"
        "notebook { border: none; }"
        "notebook tab { padding: 10px 15px; border: none; background: none; margin: 0; }"
        "notebook tab:checked { color: @theme_selected_bg_color; box-shadow: inset 0 -2px 0 @theme_selected_bg_color; }"
        
        "/* --- AI Assistant --- */"
        "#resultLabel { padding: 15px; background-color: #f1f8ff; border-radius: 6px; border: 1px solid #c8e1ff; color: #0366d6; font-weight: 600; }";

    gtk_css_provider_load_from_data(provider, css_data, -1, NULL);
    g_object_unref(provider);
}

void create_main_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Hospital Management System");
    gtk_window_maximize(GTK_WINDOW(window));
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Hospital Management");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_container_add(GTK_CONTAINER(window), notebook);

    GtkWidget *patients_tab = create_patients_tab();
    GtkWidget *patients_label = gtk_label_new("Patients");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), patients_tab, patients_label);

    GtkWidget *ai_tab = create_ai_assistant_tab();
    GtkWidget *ai_label = gtk_label_new("AI Assistant");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), ai_tab, ai_label);

    gtk_widget_show_all(window);
}

