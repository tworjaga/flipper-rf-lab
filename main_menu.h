#include "main_menu.h"
#include "../core/flipper_rf_lab.h"

#define TAG "MAIN_MENU"

static MainMenuContext_t menu_context;

// Menu item names
static const char* menu_names[] = {
    "Capture",
    "Analyze",
    "Fingerprint",
    "Spectrum",
    "Threats",
    "Research",
    "Settings",
    "About"
};

// Callback stubs
static void capture_callback(void* context) {
    FURI_LOG_I(TAG, "Capture selected");
    // Would switch to capture view
    UNUSED(context);
}

static void analyze_callback(void* context) {
    FURI_LOG_I(TAG, "Analyze selected");
    UNUSED(context);
}

static void fingerprint_callback(void* context) {
    FURI_LOG_I(TAG, "Fingerprint selected");
    UNUSED(context);
}

static void spectrum_callback(void* context) {
    FURI_LOG_I(TAG, "Spectrum selected");
    UNUSED(context);
}

static void threats_callback(void* context) {
    FURI_LOG_I(TAG, "Threats selected");
    UNUSED(context);
}

static void research_callback(void* context) {
    FURI_LOG_I(TAG, "Research selected");
    UNUSED(context);
}

static void settings_callback(void* context) {
    FURI_LOG_I(TAG, "Settings selected");
    main_menu_show_settings();
    UNUSED(context);
}

static void about_callback(void* context) {
    FURI_LOG_I(TAG, "About selected");
    UNUSED(context);
}

// Initialize main menu
void main_menu_init(ViewDispatcher* view_dispatcher) {
    memset(&menu_context, 0, sizeof(menu_context));
    menu_context.view_dispatcher = view_dispatcher;
    
    // Create submenu
    menu_context.submenu = submenu_alloc();
    
    // Add menu items
    submenu_add_item(menu_context.submenu, "RF Capture", MENU_ID_CAPTURE, 
                     capture_callback, NULL);
    submenu_add_item(menu_context.submenu, "Signal Analysis", MENU_ID_ANALYZE, 
                     analyze_callback, NULL);
    submenu_add_item(menu_context.submenu, "Fingerprinting", MENU_ID_FINGERPRINT, 
                     fingerprint_callback, NULL);
    submenu_add_item(menu_context.submenu, "Spectrum Scan", MENU_ID_SPECTRUM, 
                     spectrum_callback, NULL);
    submenu_add_item(menu_context.submenu, "Threat Model", MENU_ID_THREATS, 
                     threats_callback, NULL);
    submenu_add_item(menu_context.submenu, "Research Mode", MENU_ID_RESEARCH, 
                     research_callback, NULL);
    submenu_add_item(menu_context.submenu, "Settings", MENU_ID_SETTINGS, 
                     settings_callback, NULL);
    submenu_add_item(menu_context.submenu, "About", MENU_ID_ABOUT, 
                     about_callback, NULL);
    
    // Add submenu to view dispatcher
    view_dispatcher_add_view(view_dispatcher, VIEW_MAIN_MENU, 
                            submenu_get_view(menu_context.submenu));
    
    // Create settings list
    menu_context.settings_list = variable_item_list_alloc();
    
    // Switch to main menu
    view_dispatcher_switch_to_view(view_dispatcher, VIEW_MAIN_MENU);
    
    FURI_LOG_I(TAG, "Main menu initialized");
}

// Deinitialize main menu
void main_menu_deinit(void) {
    if(menu_context.view_dispatcher) {
        view_dispatcher_remove_view(menu_context.view_dispatcher, VIEW_MAIN_MENU);
    }
    
    if(menu_context.submenu) {
        submenu_free(menu_context.submenu);
    }
    
    if(menu_context.settings_list) {
        variable_item_list_free(menu_context.settings_list);
    }
    
    FURI_LOG_I(TAG, "Main menu deinitialized");
}

// Show main menu
void main_menu_show(void) {
    if(menu_context.view_dispatcher) {
        view_dispatcher_switch_to_view(menu_context.view_dispatcher, VIEW_MAIN_MENU);
    }
}

// Hide main menu
void main_menu_hide(void) {
    // Menu is always visible in background
}

// Set selected item
void main_menu_set_selected(uint8_t index) {
    if(menu_context.submenu) {
        submenu_set_selected_item(menu_context.submenu, index);
    }
}

// Register callback for menu item
void main_menu_register_callback(MenuItemId_t id, void (*callback)(void*), void* context) {
    // Would update callback in submenu
    UNUSED(id);
    UNUSED(callback);
    UNUSED(context);
}

// Handle input event
void main_menu_handle_input(InputEvent* event) {
    // Input is handled by view dispatcher
    UNUSED(event);
}

// Show settings submenu
void main_menu_show_settings(void) {
    if(!menu_context.settings_list) return;
    
    // Clear existing items
    variable_item_list_reset(menu_context.settings_list);
    
    // Add settings items
    VariableItem* item;
    
    item = variable_item_list_add(menu_context.settings_list, "Frequency", 4, 
                                   NULL, NULL);
    variable_item_set_current_value_index(item, 1);  // 433 MHz default
    variable_item_set_current_value_text(item, "433.92");
    
    item = variable_item_list_add(menu_context.settings_list, "Modulation", 6, 
                                   NULL, NULL);
    variable_item_set_current_value_index(item, 4);  // OOK default
    variable_item_set_current_value_text(item, "OOK");
    
    item = variable_item_list_add(menu_context.settings_list, "Data Rate", 10, 
                                   NULL, NULL);
    variable_item_set_current_value_index(item, 2);  // 2.4k default
    variable_item_set_current_value_text(item, "2.4k");
    
    item = variable_item_list_add(menu_context.settings_list, "Power", 11, 
                                   NULL, NULL);
    variable_item_set_current_value_index(item, 6);  // 0 dBm default
    variable_item_set_current_value_text(item, "0dBm");
    
    item = variable_item_list_add(menu_context.settings_list, "Bandwidth", 8, 
                                   NULL, NULL);
    variable_item_set_current_value_index(item, 3);  // 325 kHz default
    variable_item_set_current_value_text(item, "325k");
    
    // Add to view dispatcher and switch
    view_dispatcher_add_view(menu_context.view_dispatcher, VIEW_SETTINGS,
                            variable_item_list_get_view(menu_context.settings_list));
    view_dispatcher_switch_to_view(menu_context.view_dispatcher, VIEW_SETTINGS);
}

// Add item to settings
void main_menu_settings_add_item(const char* name, uint8_t values_count, 
                                  uint8_t default_value, 
                                  void (*change_callback)(uint8_t index)) {
    // Would add custom settings item
    UNUSED(name);
    UNUSED(values_count);
    UNUSED(default_value);
    UNUSED(change_callback);
}
