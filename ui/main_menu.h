#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MAIN MENU SYSTEM
// 128x64 monochrome LCD interface
// ============================================================================

#define MENU_ITEMS_MAX      8
#define MENU_NAME_LEN       16

typedef enum {
    MENU_ID_CAPTURE = 0,
    MENU_ID_ANALYZE,
    MENU_ID_FINGERPRINT,
    MENU_ID_SPECTRUM,
    MENU_ID_THREATS,
    MENU_ID_RESEARCH,
    MENU_ID_SETTINGS,
    MENU_ID_ABOUT
} MenuItemId_t;

typedef struct {
    MenuItemId_t id;
    const char* name;
    const Icon* icon;
    void (*callback)(void* context);
} MenuItem_t;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    VariableItemList* settings_list;
    
    MenuItem_t items[MENU_ITEMS_MAX];
    uint8_t item_count;
    uint8_t selected_item;
    
    void* callback_context;
} MainMenuContext_t;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

void main_menu_init(ViewDispatcher* view_dispatcher);
void main_menu_deinit(void);

void main_menu_show(void);
void main_menu_hide(void);
void main_menu_set_selected(uint8_t index);

void main_menu_register_callback(MenuItemId_t id, void (*callback)(void*), void* context);
void main_menu_handle_input(InputEvent* event);

// Settings submenu
void main_menu_show_settings(void);
void main_menu_settings_add_item(const char* name, uint8_t values_count, 
                                  uint8_t default_value, 
                                  void (*change_callback)(uint8_t index));

// ============================================================================
// VIEW IDS
// ============================================================================

typedef enum {
    VIEW_MAIN_MENU = 0,
    VIEW_CAPTURE,
    VIEW_ANALYSIS,
    VIEW_SPECTRUM,
    VIEW_FINGERPRINT,
    VIEW_THREATS,
    VIEW_RESEARCH,
    VIEW_SETTINGS,
    VIEW_ABOUT
} AppViewId_t;

#ifdef __cplusplus
}
#endif

#endif // MAIN_MENU_H
