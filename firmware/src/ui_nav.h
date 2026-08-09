#pragma once

// Creates every page and the dot indicator, then loads the Overview page.
// Call once from setup(), after display_init_panel()/display_init_touch().
void ui_nav_init();

// Refreshes the currently visible page's live content. Call every loop().
void ui_nav_update();

// Shows/hides the System Info overlay. It sits outside the normal left/right
// rotation and is reached by a short press of the BOOT button.
void ui_nav_toggle_sysinfo();
