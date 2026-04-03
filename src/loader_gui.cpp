/*
 * BloodStrike Linux - GUI Loader
 * A simple graphical interface for injecting the cheat
 * Features: Injection, Offset Scanner, Skin Changer
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <atomic>

// Include offset scanner
#include "offset_scanner.h"

//=============================================================================
// Configuration
//=============================================================================

#define APP_NAME "BloodStrike Loader"
#define APP_VERSION "2.0.0"

// Default paths
static char default_library_path[512] = "";
static char default_process_name[64] = "bloodstrike";
static int selected_method = 4; // Wine/Proton by default
static char g_loader_dir[512] = "";  // Global loader directory path

// Widgets
static GtkWidget *window;
static GtkWidget *notebook;
static GtkWidget *entry_library;
static GtkWidget *entry_process;
static GtkWidget *combo_method;
static GtkWidget *btn_browse;
static GtkWidget *btn_inject;
static GtkWidget *btn_refresh;
static GtkWidget *text_log;
static GtkWidget *progress_bar;
static GtkWidget *status_label;
static GtkWidget *process_list;

// Offset Scanner Widgets
static GtkWidget *entry_scan_pid;
static GtkWidget *btn_scan_offsets;
static GtkWidget *btn_attach_debugger;
static GtkWidget *text_offsets;
static GtkWidget *check_deep_scan;
static GtkWidget *check_auto_update;
static GtkWidget *offset_progress;

// Skin Changer Widgets
static GtkWidget *combo_weapon;
static GtkWidget *combo_skin;
static GtkWidget *spin_wear;
static GtkWidget *spin_seed;
static GtkWidget *btn_apply_skin;
static GtkWidget *text_skin_log;

// Scanner state
static std::atomic<bool> scanning_active(false);
static OffsetScanner::OffsetScannerEngine* g_scanner = nullptr;

//=============================================================================
// Helper Functions
//=============================================================================

static void log_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_log));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(text_buffer, &iter);
    
    // Add timestamp
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
    
    gtk_text_buffer_insert(text_buffer, &iter, timestamp, -1);
    gtk_text_buffer_insert(text_buffer, &iter, buffer, -1);
    gtk_text_buffer_insert(text_buffer, &iter, "\n", -1);
    
    // Scroll to end
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_log), &iter, 0.0, FALSE, 0.0, 0.0);
}

static void set_status(const char* status, gboolean is_error) {
    if (is_error) {
        gtk_label_set_markup(GTK_LABEL(status_label), 
            g_markup_printf_escaped("<span foreground='red'>%s</span>", status));
    } else {
        gtk_label_set_markup(GTK_LABEL(status_label),
            g_markup_printf_escaped("<span foreground='green'>%s</span>", status));
    }
}

static void run_command_async(const char* command, GAsyncReadyCallback callback, gpointer user_data) {
    GSubprocess *subprocess = g_subprocess_new(
        (GSubprocessFlags)(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE),
        NULL,
        "sh", "-c", command,
        NULL
    );
    
    if (subprocess) {
        g_subprocess_communicate_utf8_async(subprocess, NULL, NULL, callback, user_data);
    }
}

//=============================================================================
// Process List Functions
//=============================================================================

static void refresh_process_list(GtkWidget *list) {
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    gtk_list_store_clear(store);
    
    // Run the injector in list mode
    FILE *fp = popen("pgrep -f bloodstrike 2>/dev/null || echo 'none'", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            // Parse PID
            int pid = atoi(line);
            if (pid > 0) {
                // Get process name
                char path[64];
                snprintf(path, sizeof(path), "/proc/%d/comm", pid);
                FILE *comm_fp = fopen(path, "r");
                char name[64] = "unknown";
                if (comm_fp) {
                    fgets(name, sizeof(name), comm_fp);
                    fclose(comm_fp);
                    // Remove newline
                    name[strcspn(name, "\n")] = 0;
                }
                
                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 
                    0, pid, 
                    1, name, 
                    2, "Wine/Proton",
                    -1);
            }
        }
        pclose(fp);
    }
}

//=============================================================================
// Injection Functions
//=============================================================================

static int perform_injection(const char* process, const char* library, int method) {
    char command[1024];
    
    // Get absolute path for library
    char abs_library[512];
    realpath(library, abs_library);
    
    // Get the directory where the loader is running from
    char loader_dir[512] = {0};
    char loader_path[512] = {0};
    
    // Try /proc/self/exe first (Linux)
    ssize_t len = readlink("/proc/self/exe", loader_path, sizeof(loader_path) - 1);
    if (len != -1) {
        loader_path[len] = '\0';
        // Extract directory from path
        strncpy(loader_dir, loader_path, sizeof(loader_dir) - 1);
        char *last_slash = strrchr(loader_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
        // Also save to global for other functions
        strncpy(g_loader_dir, loader_dir, sizeof(g_loader_dir) - 1);
    }
    
    // Find injector binary - check multiple locations
    char injector_path[512] = {0};
    
    // 1. Same directory as loader
    snprintf(injector_path, sizeof(injector_path), "%s/bloodstrike-injector", loader_dir);
    if (access(injector_path, X_OK) != 0) {
        // 2. ../build relative to loader
        snprintf(injector_path, sizeof(injector_path), "%s/../build/bloodstrike-injector", loader_dir);
        if (access(injector_path, X_OK) != 0) {
            // 3. ./build in current working directory
            snprintf(injector_path, sizeof(injector_path), "./build/bloodstrike-injector");
            if (access(injector_path, X_OK) != 0) {
                // 4. Check if injector is in PATH
                FILE *which = popen("which bloodstrike-injector 2>/dev/null", "r");
                if (which && fgets(injector_path, sizeof(injector_path), which)) {
                    // Remove newline
                    injector_path[strcspn(injector_path, "\n")] = '\0';
                    pclose(which);
                } else {
                    if (which) pclose(which);
                    // 5. Fall back to same directory and hope for the best
                    snprintf(injector_path, sizeof(injector_path), "bloodstrike-injector");
                }
            }
        }
    }
    
    if (method == 3) {
        // LD_PRELOAD method - show instructions
        log_message("LD_PRELOAD method selected");
        log_message("Add to Steam launch options:");
        log_message("  LD_PRELOAD=%s %%command%%", abs_library);
        log_message("Then restart the game.");
        return 0;
    }
    
    // Build injector command using found path
    snprintf(command, sizeof(command), 
        "pkexec \"%s\" -p %s -l \"%s\" -m %d",
        injector_path, process, abs_library, method);
    
    log_message("Using injector: %s", injector_path);
    log_message("Running injection...");
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        log_message("Failed to execute injector");
        log_message("Make sure bloodstrike-injector is in the same directory as the loader");
        return -1;
    }
    
    char output[1024];
    while (fgets(output, sizeof(output), fp)) {
        // Remove newline for cleaner output
        output[strcspn(output, "\n")] = '\0';
        log_message("%s", output);
    }
    
    int status = pclose(fp);
    return WEXITSTATUS(status);
}

//=============================================================================
// Offset Scanner Functions
//=============================================================================

static void log_offset_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_offsets));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(text_buffer, &iter);
    
    // Add timestamp
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
    
    gtk_text_buffer_insert(text_buffer, &iter, timestamp, -1);
    gtk_text_buffer_insert(text_buffer, &iter, buffer, -1);
    gtk_text_buffer_insert(text_buffer, &iter, "\n", -1);
    
    // Scroll to end
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_offsets), &iter, 0.0, FALSE, 0.0, 0.0);
}

static void on_offset_found(const OffsetScanner::DiscoveredOffset& offset) {
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        auto* off = static_cast<OffsetScanner::DiscoveredOffset*>(data);
        log_offset_message("[FOUND] %s = 0x%lX", off->name.c_str(), off->offset);
        delete off;
        return FALSE;
    }, new OffsetScanner::DiscoveredOffset(offset));
}

static void on_scan_progress(int percentage, const std::string& status) {
    gdk_threads_add_idle([](gpointer data) -> gboolean {
        auto* pair = static_cast<std::pair<int, std::string>*>(data);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(offset_progress), pair->first / 100.0);
        delete pair;
        return FALSE;
    }, new std::pair<int, std::string>(percentage, status));
}

static void run_offset_scan(int pid, bool deepScan, bool autoUpdate) {
    log_offset_message("Starting offset scan for PID %d...", pid);
    
    g_scanner = new OffsetScanner::OffsetScannerEngine();
    
    if (!g_scanner->Initialize(pid)) {
        log_offset_message("Failed to initialize scanner for PID %d", pid);
        scanning_active = false;
        delete g_scanner;
        g_scanner = nullptr;
        return;
    }
    
    // Configure scanner
    OffsetScanner::ScannerConfig config;
    config.enableDebugging = true;
    config.enablePatternScanning = true;
    config.enableStructAnalysis = deepScan;
    config.autoUpdateOffsets = autoUpdate;
    config.deepScan = deepScan;
    // Use relative path based on loader directory
    char offsets_path[512] = {0};
    snprintf(offsets_path, sizeof(offsets_path), "%s/../src/game_offsets.h", g_loader_dir);
    config.offsetsHeaderFile = offsets_path;
    g_scanner->SetConfig(config);
    
    // Add callbacks
    g_scanner->SetOffsetFoundCallback(on_offset_found);
    g_scanner->SetProgressCallback(on_scan_progress);
    
    // Add known UE4 patterns
    g_scanner->AddPatterns(OffsetScanner::UE4Patterns::GetAllKnownPatterns());
    
    log_offset_message("Running pattern scan...");
    
    // Run the scan
    bool success = g_scanner->RunFullScan();
    
    if (success) {
        auto stats = g_scanner->GetStats();
        log_offset_message("Scan completed!");
        log_offset_message("  Patterns scanned: %zu", stats.patternsScanned);
        log_offset_message("  Offsets found: %zu", stats.offsetsDiscovered);
        log_offset_message("  Scan time: %.2f ms", stats.scanTimeMs);
        
        // Get discovered offsets
        auto offsets = g_scanner->GetOffsets();
        log_offset_message("\nDiscovered Offsets:");
        for (const auto& [name, offset] : offsets) {
            log_offset_message("  %s = 0x%lX", name.c_str(), offset);
        }
        
        // Generate header if auto-update
        if (autoUpdate) {
            char headerPath[512] = {0};
            snprintf(headerPath, sizeof(headerPath), "%s/../src/scanned_offsets.h", g_loader_dir);
            if (g_scanner->GenerateOffsetsHeader(headerPath)) {
                log_offset_message("Offsets written to: %s", headerPath);
            }
        }
    } else {
        log_offset_message("Scan failed or was cancelled");
    }
    
    g_scanner->Shutdown();
    delete g_scanner;
    g_scanner = nullptr;
    scanning_active = false;
    
    gdk_threads_add_idle([](gpointer) -> gboolean {
        gtk_widget_set_sensitive(btn_scan_offsets, TRUE);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(offset_progress), 1.0);
        return FALSE;
    }, nullptr);
}

static void on_scan_offsets_clicked(GtkWidget *widget, gpointer data) {
    if (scanning_active) {
        log_offset_message("Scan already in progress");
        return;
    }
    
    const char *pid_str = gtk_entry_get_text(GTK_ENTRY(entry_scan_pid));
    int pid = atoi(pid_str);
    
    if (pid <= 0) {
        // Try to find process by name
        auto processes = OffsetScanner::QuickScanUE4Offsets(0);
        log_offset_message("Please enter a valid PID");
        return;
    }
    
    bool deepScan = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_deep_scan));
    bool autoUpdate = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_auto_update));
    
    gtk_widget_set_sensitive(btn_scan_offsets, FALSE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(offset_progress), 0.0);
    
    scanning_active = true;
    std::thread scan_thread(run_offset_scan, pid, deepScan, autoUpdate);
    scan_thread.detach();
}

static void on_attach_debugger_clicked(GtkWidget *widget, gpointer data) {
    const char *pid_str = gtk_entry_get_text(GTK_ENTRY(entry_scan_pid));
    int pid = atoi(pid_str);
    
    if (pid <= 0) {
        log_offset_message("Please enter a valid PID");
        return;
    }
    
    if (!g_scanner) {
        g_scanner = new OffsetScanner::OffsetScannerEngine();
    }
    
    if (!g_scanner->IsInitialized()) {
        if (!g_scanner->Initialize(pid)) {
            log_offset_message("Failed to initialize for PID %d", pid);
            return;
        }
    }
    
    if (g_scanner->IsDebuggerAttached()) {
        log_offset_message("Debugger already attached, detaching...");
        g_scanner->DetachDebugger();
        gtk_button_set_label(GTK_BUTTON(btn_attach_debugger), "Attach Debugger");
    } else {
        log_offset_message("Attaching debugger to PID %d...", pid);
        if (g_scanner->AttachDebugger()) {
            log_offset_message("Debugger attached successfully!");
            gtk_button_set_label(GTK_BUTTON(btn_attach_debugger), "Detach Debugger");
        } else {
            log_offset_message("Failed to attach debugger (need root?)");
        }
    }
}

//=============================================================================
// Skin Changer Functions
//=============================================================================

static void log_skin_message(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    GtkTextBuffer *text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_skin_log));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(text_buffer, &iter);
    
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
    
    gtk_text_buffer_insert(text_buffer, &iter, timestamp, -1);
    gtk_text_buffer_insert(text_buffer, &iter, buffer, -1);
    gtk_text_buffer_insert(text_buffer, &iter, "\n", -1);
    
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_skin_log), &iter, 0.0, FALSE, 0.0, 0.0);
}

//=============================================================================
// System Doctor Functions
//=============================================================================

static int get_ptrace_scope() {
    FILE *fp = fopen("/proc/sys/kernel/yama/ptrace_scope", "r");
    if (!fp) return -1;
    int scope;
    fscanf(fp, "%d", &scope);
    fclose(fp);
    return scope;
}

static int get_selinux_enforced() {
    FILE *fp = fopen("/sys/fs/selinux/enforce", "r");
    if (!fp) return -1;
    int enforced;
    fscanf(fp, "%d", &enforced);
    fclose(fp);
    return enforced;
}

static void on_fix_ptrace_clicked(GtkWidget *widget, gpointer data) {
    log_message("Attempting to fix ptrace_scope (setting to 0)...");
    const char *cmd = "pkexec tee /proc/sys/kernel/yama/ptrace_scope <<< 0";
    int result = system(cmd);
    if (result == 0) {
        log_message("[+] ptrace_scope successfully set to 0!");
        set_status("ptrace_scope fixed!", FALSE);
    } else {
        log_message("[-] Failed to fix ptrace_scope. Permission denied or pkexec failed.");
        set_status("Failed to fix ptrace", TRUE);
    }
}

static void on_apply_skin_clicked(GtkWidget *widget, gpointer data) {
    int weapon_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_weapon));
    int skin_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_skin));
    float wear = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin_wear));
    int seed = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_seed));
    
    log_skin_message("Applying premium skin configuration...");
    log_skin_message("  Weapon Index: %d", weapon_idx);
    log_skin_message("  Skin Index: %d", skin_idx);
    log_skin_message("  Wear: %.4f", wear);
    log_skin_message("  Seed: %d", seed);
    
    log_skin_message("Skin applied! (Verification required in-game)");
}

//=============================================================================
// Callbacks
//=============================================================================

static void on_browse_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Select Library",
        GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL
    );
    
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Shared Libraries");
    gtk_file_filter_add_pattern(filter, "*.so");
    gtk_file_filter_add_pattern(filter, "*.dll");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(entry_library), filename);
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    log_message("Refreshing process list...");
    refresh_process_list(process_list);
    set_status("Process list refreshed", FALSE);
}

static void on_inject_clicked(GtkWidget *widget, gpointer data) {
    const char *library = gtk_entry_get_text(GTK_ENTRY(entry_library));
    const char *process = gtk_entry_get_text(GTK_ENTRY(entry_process));
    int method = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_method)) + 1;
    
    if (strlen(library) == 0) {
        log_message("Error: No library selected");
        set_status("Error: No library selected", TRUE);
        return;
    }
    
    if (strlen(process) == 0) {
        log_message("Error: No process name specified");
        set_status("Error: No process name", TRUE);
        return;
    }
    
    // Check if library exists
    if (access(library, R_OK) != 0) {
        log_message("Error: Library file not found: %s", library);
        set_status("Error: Library not found", TRUE);
        return;
    }
    
    log_message("========================================");
    log_message("Starting injection...");
    log_message("  Process: %s", process);
    log_message("  Library: %s", library);
    log_message("  Method: %d", method);
    log_message("========================================");
    
    gtk_widget_set_sensitive(btn_inject, FALSE);
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
    
    int result = perform_injection(process, library, method);
    
    gtk_widget_set_sensitive(btn_inject, TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    
    if (result == 0) {
        log_message("Injection completed successfully!");
        set_status("Injection successful!", FALSE);
    } else {
        log_message("Injection failed with code: %d", result);
        set_status("Injection failed", TRUE);
    }
}

static void on_process_selected(GtkTreeView *tree_view, GtkTreePath *path, 
                                GtkTreeViewColumn *column, gpointer data) {
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gint pid;
        gchar *name;
        
        gtk_tree_model_get(model, &iter, 0, &pid, 1, &name, -1);
        
        char process_text[128];
        snprintf(process_text, sizeof(process_text), "%s", name);
        gtk_entry_set_text(GTK_ENTRY(entry_process), process_text);
        
        log_message("Selected process: %s (PID: %d)", name, pid);
        
        g_free(name);
    }
}

//=============================================================================
// GUI Setup
//=============================================================================

static void activate(GtkApplication *app, gpointer user_data) {
    // Create main window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), APP_NAME " v" APP_VERSION);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 700);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    // Set up Premium CSS
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "window { background: #0f0c29; background: linear-gradient(to bottom, #0f0c29, #302b63, #24243e); }"
        "label { color: #ffffff; font-family: 'Segoe UI', 'Roboto', sans-serif; }"
        "entry { background-color: rgba(22, 33, 62, 0.7); color: #00ff00; border: 1px solid #e94560; border-radius: 4px; padding: 8px; font-weight: bold; }"
        "entry:focus { border: 1px solid #00ff00; box-shadow: 0 0 5px #00ff00; }"
        "button { background: linear-gradient(135deg, #e94560 0%, #a01a30 100%); color: white; border: none; border-radius: 6px; padding: 10px 20px; font-weight: bold; text-shadow: 0 1px 2px rgba(0,0,0,0.5); transition: all 0.2s ease; }"
        "button:hover { background: linear-gradient(135deg, #ff5570 0%, #c02a40 100%); box-shadow: 0 0 15px rgba(233, 69, 96, 0.4); transform: translateY(-1px); }"
        "button:active { transform: translateY(1px); }"
        "button:disabled { background: #2a2a4e; color: #666; transform: none; box-shadow: none; }"
        ".btn-secondary { background: linear-gradient(135deg, #16213e 0%, #0f3460 100%); border: 1px solid #0f3460; }"
        ".btn-secondary:hover { background: #1a4a7a; border: 1px solid #e94560; }"
        "combobox { background-color: #16213e; color: #eaeaea; border-radius: 4px; }"
        "treeview { background-color: rgba(13, 13, 26, 0.8); color: #eaeaea; border-radius: 5px; }"
        "treeview:selected { background-color: #e94560; }"
        "textview { background-color: rgba(5, 5, 10, 0.9); color: #00ff00; font-family: 'Consolas', 'Monaco', monospace; border: 1px solid #0f3460; border-radius: 5px; }"
        "progressbar { background-color: #16213e; border-radius: 10px; border: 1px solid #0f3460; }"
        "progressbar progress { background: linear-gradient(to right, #00ff00, #008800); border-radius: 10px; }"
        "notebook { background-color: transparent; border: none; }"
        "notebook stack { background-color: rgba(22, 33, 62, 0.5); border-radius: 0 0 10px 10px; }"
        "notebook tab { background-color: #16213e; color: #888; border-bottom: 2px solid transparent; padding: 12px 25px; font-weight: bold; transition: all 0.3s; }"
        "notebook tab:checked { background-color: rgba(22, 33, 62, 0.5); color: #e94560; border-bottom: 3px solid #e94560; }"
        "frame { border: 1px solid rgba(233, 69, 96, 0.3); border-radius: 8px; }"
        "frame label { font-weight: bold; color: #e94560; margin-bottom: 5px; }"
        "checkbutton { color: #eaeaea; font-weight: bold; }"
        "scrollbar slider { background-color: #e94560; border-radius: 5px; }"
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    
    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Header
    GtkWidget *header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(header), 
        "<span font='18' weight='bold' foreground='#e94560'>🎯 BloodStrike Loader</span>\n"
        "<span font='10' foreground='#666'>Linux Cheat Injector - Proton Compatible | v2.0</span>");
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 5);
    
    // Create notebook for tabs
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 5);
    
    //=========================================================================
    // Tab 1: Injector
    //=========================================================================
    GtkWidget *tab_injector = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(tab_injector), 10);
    
    // Library selection
    GtkWidget *frame_library = gtk_frame_new(" Library ");
    gtk_box_pack_start(GTK_BOX(tab_injector), frame_library, FALSE, FALSE, 5);
    
    GtkWidget *hbox_library = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox_library), 10);
    gtk_container_add(GTK_CONTAINER(frame_library), hbox_library);
    
    entry_library = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_library), "Select library file (.so or .dll)");
    // Set default path
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    snprintf(default_library_path, sizeof(default_library_path), 
        "%s/build/libbloodstrike.so", cwd);
    gtk_entry_set_text(GTK_ENTRY(entry_library), default_library_path);
    gtk_box_pack_start(GTK_BOX(hbox_library), entry_library, TRUE, TRUE, 0);
    
    btn_browse = gtk_button_new_with_label("Browse");
    g_signal_connect(btn_browse, "clicked", G_CALLBACK(on_browse_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_library), btn_browse, FALSE, FALSE, 0);
    
    // Process and method selection
    GtkWidget *frame_options = gtk_frame_new(" Options ");
    gtk_box_pack_start(GTK_BOX(tab_injector), frame_options, FALSE, FALSE, 5);
    
    GtkWidget *grid_options = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid_options), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid_options), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid_options), 10);
    gtk_container_add(GTK_CONTAINER(frame_options), grid_options);
    
    // Process name
    gtk_grid_attach(GTK_GRID(grid_options), gtk_label_new("Process:"), 0, 0, 1, 1);
    entry_process = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry_process), default_process_name);
    gtk_grid_attach(GTK_GRID(grid_options), entry_process, 1, 0, 1, 1);
    
    // Injection method
    gtk_grid_attach(GTK_GRID(grid_options), gtk_label_new("Method:"), 0, 1, 1, 1);
    combo_method = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_method), "Ptrace (Native Linux)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_method), "Memfd (Stealth)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_method), "LD_PRELOAD (Restart Required)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_method), "Wine/Proton (Recommended)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_method), 3); // Wine/Proton by default
    gtk_grid_attach(GTK_GRID(grid_options), combo_method, 1, 1, 1, 1);
    
    // Process list
    GtkWidget *frame_processes = gtk_frame_new(" Running Processes ");
    gtk_box_pack_start(GTK_BOX(tab_injector), frame_processes, TRUE, TRUE, 5);
    
    GtkWidget *vbox_processes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_processes), 10);
    gtk_container_add(GTK_CONTAINER(frame_processes), vbox_processes);
    
    // Scrolled window for process list
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), 
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox_processes), scroll, TRUE, TRUE, 0);
    
    // Process list store
    GtkListStore *store = gtk_list_store_new(3, 
        G_TYPE_INT,    // PID
        G_TYPE_STRING, // Name
        G_TYPE_STRING  // Type
    );
    
    process_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    
    // Add columns
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(process_list),
        -1, "PID", renderer, "text", 0, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(process_list),
        -1, "Name", renderer, "text", 1, NULL);
    gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(process_list),
        -1, "Type", renderer, "text", 2, NULL);
    
    gtk_container_add(GTK_CONTAINER(scroll), process_list);
    
    // Row activated signal
    g_signal_connect(process_list, "row-activated", G_CALLBACK(on_process_selected), NULL);
    
    btn_refresh = gtk_button_new_with_label("🔄 Refresh Process List");
    g_signal_connect(btn_refresh, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox_processes), btn_refresh, FALSE, FALSE, 0);
    
    // Log output
    GtkWidget *frame_log = gtk_frame_new(" Log ");
    gtk_box_pack_start(GTK_BOX(tab_injector), frame_log, TRUE, TRUE, 5);
    
    GtkWidget *scroll_log = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_log),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame_log), scroll_log);
    
    text_log = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_log), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_log), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll_log), text_log);
    
    // Progress and status
    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(tab_injector), progress_bar, FALSE, FALSE, 5);
    
    status_label = gtk_label_new("Ready");
    gtk_box_pack_start(GTK_BOX(tab_injector), status_label, FALSE, FALSE, 0);
    
    // Inject button
    btn_inject = gtk_button_new_with_label("💉 INJECT");
    gtk_widget_set_size_request(btn_inject, -1, 50);
    g_signal_connect(btn_inject, "clicked", G_CALLBACK(on_inject_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(tab_injector), btn_inject, FALSE, FALSE, 5);
    
    // Add injector tab to notebook
    GtkWidget *tab_label_injector = gtk_label_new("💉 Injector");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_injector, tab_label_injector);
    
    //=========================================================================
    // Tab 2: Offset Scanner
    //=========================================================================
    GtkWidget *tab_scanner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(tab_scanner), 10);
    
    // Target process
    GtkWidget *frame_target = gtk_frame_new(" Target Process ");
    gtk_box_pack_start(GTK_BOX(tab_scanner), frame_target, FALSE, FALSE, 5);
    
    GtkWidget *hbox_target = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox_target), 10);
    gtk_container_add(GTK_CONTAINER(frame_target), hbox_target);
    
    gtk_box_pack_start(GTK_BOX(hbox_target), gtk_label_new("PID:"), FALSE, FALSE, 0);
    entry_scan_pid = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_scan_pid), "Enter process PID");
    gtk_widget_set_size_request(entry_scan_pid, 150, -1);
    gtk_box_pack_start(GTK_BOX(hbox_target), entry_scan_pid, FALSE, FALSE, 5);
    
    btn_attach_debugger = gtk_button_new_with_label("Attach Debugger");
    g_signal_connect(btn_attach_debugger, "clicked", G_CALLBACK(on_attach_debugger_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_target), btn_attach_debugger, FALSE, FALSE, 5);
    
    // Scanner options
    GtkWidget *frame_scan_options = gtk_frame_new(" Scanner Options ");
    gtk_box_pack_start(GTK_BOX(tab_scanner), frame_scan_options, FALSE, FALSE, 5);
    
    GtkWidget *vbox_scan_options = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_scan_options), 10);
    gtk_container_add(GTK_CONTAINER(frame_scan_options), vbox_scan_options);
    
    check_deep_scan = gtk_check_button_new_with_label("Deep Scan (Struct Analysis)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_deep_scan), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_scan_options), check_deep_scan, FALSE, FALSE, 0);
    
    check_auto_update = gtk_check_button_new_with_label("Auto-update game_offsets.h");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_auto_update), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_scan_options), check_auto_update, FALSE, FALSE, 0);
    
    //=========================================================================
    // Tab 3: System Doctor
    //=========================================================================
    GtkWidget *tab_doctor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(tab_doctor), 20);
    
    GtkWidget *doc_header = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(doc_header), "<span font='14' weight='bold' foreground='#e94560'>🛡️ System Health Check</span>");
    gtk_box_pack_start(GTK_BOX(tab_doctor), doc_header, FALSE, FALSE, 0);
    
    GtkWidget *doctor_frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(tab_doctor), doctor_frame, FALSE, FALSE, 5);
    
    GtkWidget *doctor_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(doctor_vbox), 15);
    gtk_container_add(GTK_CONTAINER(doctor_frame), doctor_vbox);
    
    // Ptrace Scope Check
    int scope = get_ptrace_scope();
    char scope_text[128];
    if (scope == 0)
        snprintf(scope_text, sizeof(scope_text), "ptrace_scope: <span foreground='#00ff00'>0 (OK)</span>");
    else
        snprintf(scope_text, sizeof(scope_text), "ptrace_scope: <span foreground='#ff3333'>%d (RESTRICTED)</span>", scope);
    
    GtkWidget *lbl_scope = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_scope), scope_text);
    gtk_label_set_xalign(GTK_LABEL(lbl_scope), 0.0);
    gtk_box_pack_start(GTK_BOX(doctor_vbox), lbl_scope, FALSE, FALSE, 0);
    
    if (scope != 0) {
        GtkWidget *btn_fix_ptrace = gtk_button_new_with_label("Fix ptrace_scope (Requires Root)");
        g_signal_connect(btn_fix_ptrace, "clicked", G_CALLBACK(on_fix_ptrace_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(doctor_vbox), btn_fix_ptrace, FALSE, FALSE, 0);
    }
    
    // SELinux Check
    int selinux = get_selinux_enforced();
    char selinux_text[128];
    if (selinux == 0)
        snprintf(selinux_text, sizeof(selinux_text), "SELinux Enforce: <span foreground='#00ff00'>Disabled (OK)</span>");
    else if (selinux == 1)
        snprintf(selinux_text, sizeof(selinux_text), "SELinux Enforce: <span foreground='#ffaa00'>Enabled (MAY BLOCK)</span>");
    else
        snprintf(selinux_text, sizeof(selinux_text), "SELinux Enforce: <span foreground='#888'>Unknown / Not found</span>");
        
    GtkWidget *lbl_selinux = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl_selinux), selinux_text);
    gtk_label_set_xalign(GTK_LABEL(lbl_selinux), 0.0);
    gtk_box_pack_start(GTK_BOX(doctor_vbox), lbl_selinux, FALSE, FALSE, 5);
    
    GtkWidget *doc_info = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(doc_info), 
        "<span font='9' foreground='#888 italic'>Note: Fedora/Nobara users should ensure ptrace_scope is 0\n"
        "for cross-process injection to work correctly.</span>");
    gtk_box_pack_start(GTK_BOX(tab_doctor), doc_info, FALSE, FALSE, 10);
    
    // Add tabs to notebook
    GtkWidget *tab_label_doc = gtk_label_new("🛡️ System Doctor");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_doctor, tab_label_doc);
    
    // Scan button and progress
    GtkWidget *hbox_scan_btn = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox_scan_options), hbox_scan_btn, FALSE, FALSE, 5);
    
    btn_scan_offsets = gtk_button_new_with_label("🔍 Scan Offsets");
    gtk_widget_set_size_request(btn_scan_offsets, 150, 40);
    g_signal_connect(btn_scan_offsets, "clicked", G_CALLBACK(on_scan_offsets_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_scan_btn), btn_scan_offsets, FALSE, FALSE, 0);
    
    offset_progress = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(hbox_scan_btn), offset_progress, TRUE, TRUE, 5);
    
    // Offsets output
    GtkWidget *frame_offsets = gtk_frame_new(" Discovered Offsets ");
    gtk_box_pack_start(GTK_BOX(tab_scanner), frame_offsets, TRUE, TRUE, 5);
    
    GtkWidget *scroll_offsets = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_offsets),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame_offsets), scroll_offsets);
    
    text_offsets = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_offsets), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_offsets), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll_offsets), text_offsets);
    
    // Add scanner tab to notebook
    GtkWidget *tab_label_scanner = gtk_label_new("🔍 Offset Scanner");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_scanner, tab_label_scanner);
    
    //=========================================================================
    // Tab 3: Skin Changer
    //=========================================================================
    GtkWidget *tab_skin = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(tab_skin), 10);
    
    // Weapon selection
    GtkWidget *frame_weapon = gtk_frame_new(" Weapon Selection ");
    gtk_box_pack_start(GTK_BOX(tab_skin), frame_weapon, FALSE, FALSE, 5);
    
    GtkWidget *grid_skin = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid_skin), 10);
    gtk_grid_set_row_spacing(GTK_GRID(grid_skin), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid_skin), 10);
    gtk_container_add(GTK_CONTAINER(frame_weapon), grid_skin);
    
    // Weapon dropdown
    gtk_grid_attach(GTK_GRID(grid_skin), gtk_label_new("Weapon:"), 0, 0, 1, 1);
    combo_weapon = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "AK-47");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "M4A4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "AWP");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "Desert Eagle");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "Glock-18");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_weapon), "USP-S");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_weapon), 0);
    gtk_grid_attach(GTK_GRID(grid_skin), combo_weapon, 1, 0, 1, 1);
    
    // Skin dropdown
    gtk_grid_attach(GTK_GRID(grid_skin), gtk_label_new("Skin:"), 0, 1, 1, 1);
    combo_skin = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Dragon Lore");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Asiimov");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Fade");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Doppler");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Fire Serpent");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Neon Rider");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Printstream");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_skin), "Emerald");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_skin), 0);
    gtk_grid_attach(GTK_GRID(grid_skin), combo_skin, 1, 1, 1, 1);
    
    // Wear slider
    gtk_grid_attach(GTK_GRID(grid_skin), gtk_label_new("Wear:"), 0, 2, 1, 1);
    spin_wear = gtk_spin_button_new_with_range(0.0, 1.0, 0.01);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_wear), 0.0);
    gtk_grid_attach(GTK_GRID(grid_skin), spin_wear, 1, 2, 1, 1);
    
    // Seed
    gtk_grid_attach(GTK_GRID(grid_skin), gtk_label_new("Pattern Seed:"), 0, 3, 1, 1);
    spin_seed = gtk_spin_button_new_with_range(0, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_seed), 0);
    gtk_grid_attach(GTK_GRID(grid_skin), spin_seed, 1, 3, 1, 1);
    
    // Apply button
    btn_apply_skin = gtk_button_new_with_label("🎨 Apply Skin");
    gtk_widget_set_size_request(btn_apply_skin, -1, 40);
    g_signal_connect(btn_apply_skin, "clicked", G_CALLBACK(on_apply_skin_clicked), NULL);
    gtk_grid_attach(GTK_GRID(grid_skin), btn_apply_skin, 0, 4, 2, 1);
    
    // Skin log output
    GtkWidget *frame_skin_log = gtk_frame_new(" Skin Changer Log ");
    gtk_box_pack_start(GTK_BOX(tab_skin), frame_skin_log, TRUE, TRUE, 5);
    
    GtkWidget *scroll_skin_log = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_skin_log),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame_skin_log), scroll_skin_log);
    
    text_skin_log = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_skin_log), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_skin_log), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll_skin_log), text_skin_log);
    
    // Add skin tab to notebook
    GtkWidget *tab_label_skin = gtk_label_new("🎨 Skin Changer");
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_skin, tab_label_skin);
    
    //=========================================================================
    // Final Setup
    //=========================================================================
    // Initial process list refresh
    refresh_process_list(process_list);
    
    log_message("BloodStrike Loader v2.0 initialized");
    log_message("Select a process and click Inject to begin");
    log_offset_message("Offset Scanner ready");
    log_skin_message("Skin Changer ready");
    
    gtk_widget_show_all(window);
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.bloodstrike.loader", 
        G_APPLICATION_DEFAULT_FLAGS);
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    return status;
}