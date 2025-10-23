# Oracle: The Champions of Arcadia
## SDL3 GUI Development Plan

**Target Platforms**: Windows (MSYS2), Linux (Arch), iOS (tablets), Android (tablets)  
**Development Environment**: Geany IDE, GCC compiler, Python for tools  
**Code Constraints**: Functions ≤30 lines, Files ≤500 lines

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Phase 1: Architecture & Design Planning](#phase-1-architecture--design-planning)
3. [Phase 2: Technical Planning](#phase-2-technical-planning)
4. [Phase 3: Input System Design](#phase-3-input-system-design)
5. [Phase 4: Card Rendering System](#phase-4-card-rendering-system)
6. [Phase 5: Font Management](#phase-5-font-management)
7. [Phase 6: Asset Pipeline](#phase-6-asset-pipeline)
8. [Phase 7: Configuration System](#phase-7-configuration-system)
9. [Phase 8: Desktop Enhancements](#phase-8-desktop-enhancements)
10. [Phase 9: Platform-Specific Considerations](#phase-9-platform-specific-considerations)
11. [Phase 10: Development Roadmap](#phase-10-development-roadmap)
12. [Code Examples](#code-examples)
13. [Order Symbol Design](#order-symbol-design)

---

## Architecture Overview

### Core Principles

- **Separate game logic from presentation** - Existing game state code works independently
- **Render abstraction layer** - All drawing through thin API that SDL3 implements
- **Event system** - Mouse/touch events translated to game actions
- **State machine** - Menu → Setup → Game → Battle → Results
- **Modular design** - Each function under 30 lines, files under 500 lines

### Directory Structure

```
oracle/
├── src/
│   ├── core/          # Game logic (platform-agnostic)
│   │   ├── combat.c/h
│   │   ├── game_state.c/h
│   │   ├── game_types.h
│   │   ├── game_constants.c/h
│   │   └── ...
│   ├── gui/           # SDL3 rendering & UI
│   │   ├── renderer.c/h
│   │   ├── card_renderer.c/h
│   │   ├── texture_cache.c/h
│   │   ├── font_manager.c/h
│   │   ├── ui_elements.c/h
│   │   ├── layout.c/h
│   │   ├── input.c/h
│   │   ├── animation.c/h
│   │   ├── context_menu.c/h
│   │   └── tooltip.c/h
│   ├── platform/      # Platform-specific code
│   │   ├── platform_windows.c
│   │   ├── platform_linux.c
│   │   ├── platform_ios.m
│   │   └── platform_android.c
│   └── main.c
├── assets/
│   ├── cards/
│   │   ├── artwork/              # Champion portraits
│   │   ├── frames/               # Card frame templates
│   │   └── borders/              # Base card shapes
│   ├── icons/
│   │   ├── species/              # 15 species icons
│   │   ├── orders/               # 5 order symbols
│   │   └── ui/                   # Sword, hexagon, etc.
│   ├── fonts/
│   │   ├── title.ttf
│   │   ├── stats.ttf
│   │   └── ui.ttf
│   └── ui/
│       ├── buttons/
│       └── backgrounds/
├── build/
└── tools/             # Python build/asset tools
```

---

## Phase 1: Architecture & Design Planning

### 1.1 Screen Layout Strategy

Based on the tablet mockup, the interface requires:

- **Top info bar**: Energy, Lunas, and other game info
- **Opponent's discard pile** (left)
- **Combat zone** (center, 3 card slots for attacker, 3 for defender)
- **Player's discard pile** (right)
- **Player's hand** (bottom, 9 card slots)
- **Messages and buttons** (bottom center)

**Layout Design:**
- Use normalized coordinates (0.0-1.0) internally
- Scale to actual pixels based on screen resolution
- Touch targets: minimum 44x44pt (iOS), 48x48dp (Android)

### 1.2 Multi-Platform Considerations

| Platform | Input | Resolution | DPI Scaling | Build Tool |
|----------|-------|------------|-------------|------------|
| Windows | Mouse/Touch | Variable | 96-192 DPI | MSYS2/gcc |
| Linux | Mouse/Touch | Variable | 96-192 DPI | gcc |
| iOS | Touch only | Retina | 2x-3x | Xcode (+ C core) |
| Android | Touch only | Variable | mdpi-xxxhdpi | NDK/gradle |

---

## Phase 2: Technical Planning

### 2.1 SDL3 Setup & Build System

**Recommended approach:**
1. **CMake as build system** - Works across all platforms
2. **vcpkg or manual SDL3 builds** - For Windows/Linux
3. **iOS**: Embed SDL3 as framework
4. **Android**: Use SDL3's Android project template

**Alternative (Makefile-based):**
- Separate Makefiles for Windows (MSYS2) and Linux
- Python scripts to generate platform-specific builds

**Example Makefile (Linux/Windows):**
```makefile
# Makefile for Linux/MSYS2
CC = gcc
CFLAGS = -Wall -O2 `sdl3-config --cflags`
LIBS = `sdl3-config --libs` -lSDL3_image -lSDL3_ttf

SOURCES = src/main.c src/gui/renderer.c src/gui/card_renderer.c \
          src/core/game_state.c src/core/combat.c
OBJECTS = $(SOURCES:.c=.o)

oracle: $(OBJECTS)
	$(CC) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) oracle
```

**CMake Example:**
```cmake
cmake_minimum_required(VERSION 3.20)
project(Oracle)

find_package(SDL3 REQUIRED)
find_package(SDL3_image REQUIRED)
find_package(SDL3_ttf REQUIRED)

add_executable(oracle 
    src/main.c
    src/gui/renderer.c
    src/gui/card_renderer.c
    src/core/game_state.c
)

target_link_libraries(oracle 
    SDL3::SDL3 
    SDL3_image::SDL3_image 
    SDL3_ttf::SDL3_ttf
)
```

### 2.2 Key SDL3 Components

```c
// Core SDL3 modules needed
SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)
SDL_CreateWindow()
SDL_CreateRenderer()
SDL_RenderTexture()      // For cards
SDL_RenderGeometry()     // For UI elements
SDL_TTF                  // For text
SDL_Image                // For PNG cards
```

### 2.3 Rendering Pipeline Design

**Key principle**: Keep functions under 30 lines by breaking into small pieces

```c
// High-level rendering structure
void render_game_board(Renderer* r, GameState* g);
void render_hand(Renderer* r, Card* cards, int count);
void render_battle_zone(Renderer* r, Card* atk, Card* def);
void render_ui_overlay(Renderer* r, int energy, int lunas);
void render_button(Renderer* r, Button* btn);
```

---

## Phase 3: Input System Design

### 3.1 Unified Input Architecture

```c
// input.h - Platform-agnostic input abstraction

typedef enum {
    INPUT_NONE,
    INPUT_POINTER,    // Mouse or touch
    INPUT_KEYBOARD,
    INPUT_GAMEPAD     // Future expansion
} InputType;

typedef enum {
    ACTION_SELECT_CARD_1,
    ACTION_SELECT_CARD_2,
    ACTION_SELECT_CARD_3,
    ACTION_PLAY_CARDS,
    ACTION_PASS_TURN,
    ACTION_DRAW_CARD,
    ACTION_NEXT_CARD,
    ACTION_PREV_CARD,
    ACTION_CANCEL,
    ACTION_CONFIRM,
    ACTION_SHOW_DISCARD,
    ACTION_SHOW_DECK_INFO,
    ACTION_QUICK_SAVE,
    ACTION_COUNT
} GameAction;

typedef struct {
    GameAction action;
    int param;        // e.g., card index
    float x, y;       // For pointer inputs (normalized)
} InputEvent;
```

### 3.2 Keyboard Shortcut Scheme (Desktop Only)

**Combat Actions:**
- `1-9` - Select card from hand (position)
- `SPACE` - Play selected cards / Confirm action
- `ENTER` - End turn / Pass
- `ESC` - Cancel selection / Back to menu

**Card Management:**
- `TAB` - Cycle through cards in hand
- `SHIFT+TAB` - Cycle backwards
- `D` - Show detailed view of selected card

**Information:**
- `I` - Show opponent's discard pile
- `P` - Show own discard pile
- `H` - Show hand (if hidden)
- `S` - Game statistics/state

**Quick Actions:**
- `R` - Draw/Recall card (when applicable)
- `E` - Exchange champion (when applicable)

**Debug (development only):**
- `F1` - Toggle FPS counter
- `F12` - Screenshot

### 3.3 Input Handler Implementation

```c
// input.c

static KeyBinding key_map[ACTION_COUNT];

void init_input_system(void) {
    // Default bindings
    key_map[ACTION_SELECT_CARD_1] = SDLK_1;
    key_map[ACTION_SELECT_CARD_2] = SDLK_2;
    key_map[ACTION_SELECT_CARD_3] = SDLK_3;
    key_map[ACTION_PLAY_CARDS] = SDLK_SPACE;
    key_map[ACTION_PASS_TURN] = SDLK_RETURN;
    key_map[ACTION_CANCEL] = SDLK_ESCAPE;
    // ... load from config file
}

GameAction map_key_to_action(SDL_Keycode key) {
    for (int i = 0; i < ACTION_COUNT; i++) {
        if (key_map[i] == key) return i;
    }
    return ACTION_NONE;
}

bool process_keyboard_event(SDL_Event* e, InputEvent* out) {
    if (e->type != SDL_EVENT_KEY_DOWN) return false;
    
    out->action = map_key_to_action(e->key.key);
    out->param = 0;
    
    // Special cases: number keys for card selection
    if (e->key.key >= SDLK_1 && e->key.key <= SDLK_9) {
        out->param = e->key.key - SDLK_1;
    }
    
    return out->action != ACTION_NONE;
}
```

### 3.4 Touch Input Abstraction

```c
typedef struct {
    float x, y;           // Normalized 0.0-1.0
    bool is_down;
    int finger_id;
} TouchInput;

void handle_input(GameState* g, TouchInput* touch);
```

---

## Phase 4: Card Rendering System

### 4.1 Card Data Structure

Based on your actual card design and existing fullDeck array:

```c
// card.h

typedef struct {
    int id;                    // Index in fullDeck
    char name[32];             // Champion name (from names artifact)
    Species species;
    CardColor color;
    char rank;                 // 'A' to 'E' (order)
    uint8_t defense_dice;      // d4, d6, d8, d12, d20
    uint8_t attack_base;       // Base attack value
    int cost;                  // Luna cost
    char artwork_file[64];     // e.g., "champions/lof.png"
    char species_icon_path[64]; // e.g., "icons/dwarf.png"
    char order_symbol_path[64]; // Shield icon for order
    bool is_champion;          // vs draw/exchange cards
} CardData;

typedef struct {
    CardData data;
    SDL_Texture* artwork;      // Cached texture
    bool is_selected;
    float anim_time;           // For animations
} Card;
```

### 4.2 Card Layout (Based on Your Design)

**Your card structure from the Lof example:**

1. **Top-left**: Hexagonal cost indicator
2. **Left sidebar (top)**:
   - Defense dice notation (e.g., "d4")
   - Plus sign (+)
   - Attack base (e.g., "0")
   - Sword icon
3. **Center**: Large artwork area
4. **Left sidebar (bottom)**:
   - Species icon
   - Small circle (order indicator)
   - Order shield symbol
   - Defense dice (repeated)
5. **Bottom banner**: Champion name
6. **Border**: Color-coded (Indigo, Orange, Red)

### 4.3 Card Rendering Pipeline

**Strategy:** Composite cards in layers at runtime

```
Layer 1: Colored border/frame (based on card color)
Layer 2: Central artwork area (champion portrait)
Layer 3: Left sidebar with stats (dice, attack, sword)
Layer 4: Species icon and order symbols
Layer 5: Bottom name banner
Layer 6: Cost hexagon (top-left overlay)
Layer 7: Selection highlight (if selected)
```

### 4.4 Complete Card Renderer

```c
// card_renderer.c

void render_champion_card_complete(SDL_Renderer* r, Card* card,
                                   int x, int y, int w, int h,
                                   FontManager* fm, TextureCache* tc) {
    render_card_border(r, card->data.color, x, y, w, h);
    render_card_artwork(r, card, x, y, w, h);
    render_left_sidebar(r, card, x, y, w, h, fm);
    render_species_and_order(r, tc, card, x, y, w, h);
    render_name_banner(r, fm, card, x, y, w, h);
    render_cost_hexagon(r, fm, card, x, y);
    if (card->is_selected) render_selection(r, x, y, w, h);
}

void render_card_border(SDL_Renderer* r, CardColor color,
                       int x, int y, int w, int h) {
    SDL_Color bg = get_color_for_card(color);
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
    SDL_FRect border = {x, y, w, h};
    SDL_RenderFillRect(r, &border);
    
    // Inner frame
    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    SDL_FRect inner = {x+6, y+6, w-12, h-12};
    SDL_RenderFillRect(r, &inner);
}

void render_card_artwork(SDL_Renderer* r, Card* card,
                         int x, int y, int w, int h) {
    if (!card->artwork) return;
    
    // Artwork occupies the large central rectangle
    int sidebar_width = w * 0.20;  // 20% for left sidebar
    SDL_FRect art_rect = {
        x + sidebar_width + 8,
        y + 50,  // Below cost hexagon
        w - sidebar_width - 16,
        h - 120  // Leave room for name banner
    };
    SDL_RenderTexture(r, card->artwork, NULL, &art_rect);
}

void render_left_sidebar(SDL_Renderer* r, Card* card,
                        int x, int y, int w, int h,
                        FontManager* fm) {
    int sidebar_x = x + 12;
    int sidebar_width = w * 0.15;
    SDL_Color black = {0, 0, 0, 255};
    
    // Defense dice at top
    char dice_str[8];
    snprintf(dice_str, 8, "d%d", card->data.defense_dice);
    draw_text(r, fm->fonts[FONT_CARD_STATS], dice_str,
             sidebar_x, y + 40, black);
    
    // Plus sign
    draw_text(r, fm->fonts[FONT_CARD_STATS], "+",
             sidebar_x + 15, y + 70, black);
    
    // Attack base
    char atk_str[8];
    snprintf(atk_str, 8, "%d", card->data.attack_base);
    draw_text(r, fm->fonts[FONT_CARD_STATS], atk_str,
             sidebar_x + 10, y + 100, black);
    
    // Sword icon rendering would go here
    render_sword_icon(r, sidebar_x, y + 130);
}

void render_species_and_order(SDL_Renderer* r, TextureCache* tc,
                              Card* card, int x, int y, int w, int h) {
    int sidebar_x = x + 12;
    int bottom_y = y + h - 180;
    
    // Species icon
    SDL_Texture* species_tex = get_texture(tc, 
                                          card->data.species_icon_path);
    if (species_tex) {
        SDL_FRect dst = {sidebar_x, bottom_y, 48, 48};
        SDL_RenderTexture(r, species_tex, NULL, &dst);
    }
    
    // Small circle indicator
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    // SDL_RenderFillCircle implementation needed
    
    // Order shield/symbol
    SDL_Texture* order_tex = get_texture(tc, 
                                        card->data.order_symbol_path);
    if (order_tex) {
        SDL_FRect dst = {sidebar_x, bottom_y + 60, 48, 50};
        SDL_RenderTexture(r, order_tex, NULL, &dst);
    }
    
    // Defense dice repeated at bottom
    char dice_str[8];
    snprintf(dice_str, 8, "d%d", card->data.defense_dice);
    draw_text(r, /* font */, dice_str, sidebar_x, bottom_y + 120,
             (SDL_Color){0,0,0,255});
}

void render_name_banner(SDL_Renderer* r, FontManager* fm,
                       Card* card, int x, int y, int w, int h) {
    // Banner background
    SDL_SetRenderDrawColor(r, 245, 245, 220, 255);  // Beige
    SDL_FRect banner = {x + 6, y + h - 60, w - 12, 50};
    SDL_RenderFillRect(r, &banner);
    
    // Champion name (centered)
    draw_text_centered(r, fm->fonts[FONT_CARD_TITLE],
                      card->data.name, 
                      x + w/2, y + h - 35,
                      (SDL_Color){0, 0, 0, 255});
}

void render_cost_hexagon(SDL_Renderer* r, FontManager* fm,
                        Card* card, int x, int y) {
    // Hexagon background (could be a texture)
    // For now, simple rectangle
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_FRect hex = {x + 8, y + 8, 50, 44};
    SDL_RenderFillRect(r, &hex);
    
    // Cost number
    char cost_str[8];
    snprintf(cost_str, 8, "%d", card->data.cost);
    draw_text_centered(r, fm->fonts[FONT_CARD_STATS],
                      cost_str, x + 33, y + 30,
                      (SDL_Color){0, 0, 0, 255});
}

void render_selection(SDL_Renderer* r, int x, int y, int w, int h) {
    // Pulsing border effect
    static float pulse = 0.0f;
    pulse += 0.05f;
    
    float alpha = 150 + 105 * sinf(pulse);
    SDL_SetRenderDrawColor(r, 255, 255, 100, (int)alpha);
    
    // Draw thick border
    for (int i = 0; i < 4; i++) {
        SDL_FRect border = {x-i, y-i, w+i*2, h+i*2};
        SDL_RenderRect(r, &border);
    }
}

SDL_Color get_color_for_card(CardColor color) {
    switch(color) {
        case COLOR_INDIGO:  return (SDL_Color){75, 0, 130, 255};
        case COLOR_ORANGE:  return (SDL_Color){255, 140, 0, 255};
        case COLOR_RED:     return (SDL_Color){220, 20, 60, 255};
        default:            return (SDL_Color){128, 128, 128, 255};
    }
}
```

---

## Phase 5: Font Management

### 5.1 Font Types

```c
// font_manager.h

typedef enum {
    FONT_CARD_TITLE,       // Champion name
    FONT_CARD_STATS,       // Attack/Defense numbers
    FONT_CARD_FLAVOR,      // Flavor text (future)
    FONT_UI_NORMAL,        // UI elements
    FONT_UI_LARGE,         // Buttons, headers
    FONT_UI_SMALL,         // Tooltips
    FONT_COUNT
} FontType;

typedef struct {
    TTF_Font* fonts[FONT_COUNT];
    char font_paths[FONT_COUNT][128];
    int font_sizes[FONT_COUNT];
} FontManager;
```

### 5.2 Font Recommendations

**For Card Titles (Champion Names):**
- **Cinzel** - Elegant serif, medieval feel
- **Almendra** - Fantasy-style serif
- **Libre Baskerville** - Classic readability

**For Stats/Numbers:**
- **Roboto Mono** - Clear monospaced digits
- **Source Code Pro** - Excellent number readability
- **Inconsolata** - Clean monospace

**For UI:**
- **Roboto** - Clean, modern sans-serif
- **Open Sans** - Highly readable
- **Lato** - Professional appearance

All available from [Google Fonts](https://fonts.google.com) under open licenses.

### 5.3 Font Manager Implementation

```c
// font_manager.c

void init_font_manager(FontManager* fm, float dpi_scale) {
    // Default configuration
    strcpy(fm->font_paths[FONT_CARD_TITLE], 
           "assets/fonts/title.ttf");
    strcpy(fm->font_paths[FONT_CARD_STATS], 
           "assets/fonts/stats.ttf");
    strcpy(fm->font_paths[FONT_UI_NORMAL], 
           "assets/fonts/ui.ttf");
    
    // Scale sizes for DPI
    fm->font_sizes[FONT_CARD_TITLE] = (int)(24 * dpi_scale);
    fm->font_sizes[FONT_CARD_STATS] = (int)(28 * dpi_scale);
    fm->font_sizes[FONT_UI_NORMAL] = (int)(16 * dpi_scale);
    fm->font_sizes[FONT_UI_LARGE] = (int)(20 * dpi_scale);
    fm->font_sizes[FONT_UI_SMALL] = (int)(12 * dpi_scale);
    
    load_fonts(fm);
}

void load_fonts(FontManager* fm) {
    for (int i = 0; i < FONT_COUNT; i++) {
        fm->fonts[i] = TTF_OpenFont(fm->font_paths[i], 
                                    fm->font_sizes[i]);
        if (!fm->fonts[i]) {
            printf("Failed to load font: %s\n", fm->font_paths[i]);
        }
    }
}

void cleanup_font_manager(FontManager* fm) {
    for (int i = 0; i < FONT_COUNT; i++) {
        if (fm->fonts[i]) TTF_CloseFont(fm->fonts[i]);
    }
}

TTF_Font* load_font_with_fallback(const char* primary,
                                   const char* fallback,
                                   int size) {
    TTF_Font* font = TTF_OpenFont(primary, size);
    if (font) return font;
    
    printf("Failed to load %s, trying fallback\n", primary);
    font = TTF_OpenFont(fallback, size);
    if (font) return font;
    
    printf("Failed to load fallback %s\n", fallback);
    return NULL;
}
```

### 5.4 Text Rendering Utilities

```c
// text_render.c

SDL_Texture* render_text(SDL_Renderer* r, TTF_Font* font,
                         const char* text, SDL_Color color) {
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return NULL;
    
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_DestroySurface(surf);
    return tex;
}

void draw_text(SDL_Renderer* r, TTF_Font* font,
               const char* text, int x, int y, SDL_Color color) {
    SDL_Texture* tex = render_text(r, font, text, color);
    if (!tex) return;
    
    int w, h;
    SDL_QueryTexture(tex, NULL, NULL, &w, &h);
    SDL_FRect dst = {x, y, w, h};
    SDL_RenderTexture(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

void draw_text_centered(SDL_Renderer* r, TTF_Font* font,
                        const char* text, int cx, int cy,
                        SDL_Color color) {
    int w, h;
    TTF_SizeText(font, text, &w, &h);
    draw_text(r, font, text, cx - w/2, cy - h/2, color);
}
```

---

## Phase 6: Asset Pipeline

### 6.1 Asset Organization

```
assets/
├── cards/
│   ├── artwork/              # High-res champion images
│   │   ├── lof.png
│   │   ├── fulgur.png
│   │   ├── furial.png
│   │   └── ... (102 champions)
│   ├── frames/               # Card frame templates
│   │   ├── frame_red.png
│   │   ├── frame_indigo.png
│   │   └── frame_orange.png
│   └── borders/
│       └── card_template.png
├── icons/
│   ├── species/              # 15 species icons
│   │   ├── human.png
│   │   ├── elf.png
│   │   ├── dwarf.png
│   │   ├── orc.png
│   │   ├── goblin.png
│   │   ├── dragon.png
│   │   ├── hobbit.png
│   │   ├── centaur.png
│   │   ├── minotaur.png
│   │   ├── aven.png
│   │   ├── cyclops.png
│   │   ├── faun.png
│   │   ├── fairy.png
│   │   ├── koatl.png
│   │   └── lycan.png
│   ├── orders/               # 5 order symbols
│   │   ├── order_a_sun.png
│   │   ├── order_b_leaf.png
│   │   ├── order_c_flame.png
│   │   ├── order_d_star.png
│   │   └── order_e_crescent.png
│   └── ui/
│       ├── sword.png
│       ├── hexagon.png
│       └── ...
└── fonts/
    ├── title.ttf            # For card names
    ├── stats.ttf            # For numbers (monospace)
    └── ui.ttf               # General UI
```

### 6.2 Card Asset Requirements

From game specifications:
- **120 card images** total
  - 102 champions
  - 9 draw cards (2 types)
  - 3 exchange cards
- **Card dimensions**: 65mm × 86mm (656×869 pixels at 300 DPI)

**Recommended workflow:**
1. Source assets at high resolution (2048px height)
2. Generate @1x, @2x, @3x versions for different DPI
3. Sprite sheets for better loading performance

### 6.3 Python Asset Generation Tool

```python
# tools/generate_assets.py

from PIL import Image
import os

def create_card_frame_template(color, width=656, height=869):
    """Generate base card frame template"""
    img = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Color mapping
    colors = {
        'red': (220, 20, 60),
        'indigo': (75, 0, 130),
        'orange': (255, 140, 0)
    }
    
    # Draw border
    border_color = colors.get(color, (100, 100, 100))
    draw.rectangle([0, 0, width-1, height-1], 
                   outline=border_color, width=8)
    
    return img

def resize_asset_for_dpi(input_path, output_dir, scales=[1.0, 2.0, 3.0]):
    """Generate multiple DPI versions of an asset"""
    img = Image.open(input_path)
    base_name = os.path.splitext(os.path.basename(input_path))[0]
    
    for scale in scales:
        new_size = (int(img.width * scale), int(img.height * scale))
        resized = img.resize(new_size, Image.LANCZOS)
        
        suffix = f"@{int(scale)}x" if scale != 1.0 else ""
        output_path = os.path.join(output_dir, f"{base_name}{suffix}.png")
        resized.save(output_path)
        print(f"Generated: {output_path}")

def generate_all_frames():
    """Generate frame templates for each color"""
    os.makedirs('assets/cards/frames', exist_ok=True)
    for color in ['red', 'indigo', 'orange']:
        img = create_card_frame_template(color)
        img.save(f'assets/cards/frames/frame_{color}.png')
        print(f"Generated frame_{color}.png")

def validate_card_assets():
    """Ensure all 102 champion artworks exist"""
    champion_names = [
        "lof", "fulgur", "furial", # ... etc
    ]
    
    missing = []
    for name in champion_names:
        path = f"assets/cards/artwork/{name}.png"
        if not os.path.exists(path):
            missing.append(path)
    
    if missing:
        print("Missing artwork files:")
        for m in missing:
            print(f"  - {m}")
        return False
    return True

if __name__ == '__main__':
    generate_all_frames()
    validate_card_assets()
```

### 6.4 Texture Cache System

```c
// texture_cache.h

typedef struct {
    char path[128];
    SDL_Texture* texture;
} CachedTexture;

typedef struct {
    CachedTexture* textures;
    int count;
    int capacity;
    SDL_Renderer* renderer;
} TextureCache;

TextureCache* create_texture_cache(SDL_Renderer* r, int initial_capacity);
SDL_Texture* get_texture(TextureCache* tc, const char* path);
void cache_all_species_icons(TextureCache* tc);
void cache_all_order_symbols(TextureCache* tc);
void free_texture_cache(TextureCache* tc);
```

```c
// texture_cache.c

TextureCache* create_texture_cache(SDL_Renderer* r, int capacity) {
    TextureCache* tc = malloc(sizeof(TextureCache));
    tc->textures = malloc(sizeof(CachedTexture) * capacity);
    tc->count = 0;
    tc->capacity = capacity;
    tc->renderer = r;
    return tc;
}

SDL_Texture* get_texture(TextureCache* tc, const char* path) {
    // Check if already cached
    for (int i = 0; i < tc->count; i++) {
        if (strcmp(tc->textures[i].path, path) == 0) {
            return tc->textures[i].texture;
        }
    }
    
    // Load new texture
    SDL_Texture* tex = IMG_LoadTexture(tc->renderer, path);
    if (!tex) {
        printf("Failed to load texture: %s\n", path);
        return NULL;
    }
    
    // Add to cache
    if (tc->count < tc->capacity) {
        strcpy(tc->textures[tc->count].path, path);
        tc->textures[tc->count].texture = tex;
        tc->count++;
    }
    
    return tex;
}

void cache_all_species_icons(TextureCache* tc) {
    const char* species[] = {
        "human", "elf", "dwarf", "orc", "goblin",
        "dragon", "hobbit", "centaur", "minotaur", "aven",
        "cyclops", "faun", "fairy", "koatl", "lycan"
    };
    
    for (int i = 0; i < 15; i++) {
        char path[128];
        snprintf(path, 128, "assets/icons/species/%s.png", species[i]);
        get_texture(tc, path);
    }
}

void free_texture_cache(TextureCache* tc) {
    for (int i = 0; i < tc->count; i++) {
        SDL_DestroyTexture(tc->textures[i].texture);
    }
    free(tc->textures);
    free(tc);
}
```

---

## Phase 7: Configuration System

### 7.1 User Preferences Structure

```c
// config.h

typedef struct {
    // Graphics
    int window_width;
    int window_height;
    bool fullscreen;
    bool vsync;
    float ui_scale;
    
    // Fonts
    char card_title_font[128];
    char card_stats_font[128];
    char ui_font[128];
    int font_size_multiplier;  // 80-120%
    
    // Input
    KeyBinding keybindings[ACTION_COUNT];
    bool enable_shortcuts;
    float mouse_sensitivity;
    
    // Audio
    int music_volume;
    int sfx_volume;
    
    // Gameplay
    bool show_tooltips;
    bool confirm_actions;
    int animation_speed;  // 0=none, 1=fast, 2=normal, 3=slow
} GameConfig;
```

### 7.2 Configuration File Format (INI)

```ini
[Graphics]
width=1280
height=720
fullscreen=false
vsync=true
ui_scale=1.0

[Fonts]
card_title_font=assets/fonts/cinzel.ttf
card_stats_font=assets/fonts/roboto_mono.ttf
ui_font=assets/fonts/roboto.ttf
font_size_multiplier=100

[Input]
enable_shortcuts=true
key_select_card_1=49
key_select_card_2=50
key_play_cards=32
key_pass_turn=13

[Audio]
music_volume=80
sfx_volume=70

[Gameplay]
show_tooltips=true
confirm_actions=false
animation_speed=2
```

### 7.3 Config Loader Implementation

```c
// config.c

void set_default_config(GameConfig* cfg) {
    cfg->window_width = 1280;
    cfg->window_height = 720;
    cfg->fullscreen = false;
    cfg->vsync = true;
    cfg->ui_scale = 1.0f;
    
    strcpy(cfg->card_title_font, "assets/fonts/title.ttf");
    strcpy(cfg->card_stats_font, "assets/fonts/stats.ttf");
    strcpy(cfg->ui_font, "assets/fonts/ui.ttf");
    cfg->font_size_multiplier = 100;
    
    cfg->enable_shortcuts = true;
    cfg->music_volume = 80;
    cfg->sfx_volume = 70;
    cfg->show_tooltips = true;
    cfg->confirm_actions = false;
    cfg->animation_speed = 2;
}

void load_config(GameConfig* cfg, const char* path) {
    set_default_config(cfg);
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf("Config not found, using defaults\n");
        return;
    }
    
    parse_config_file(f, cfg);
    fclose(f);
}

void parse_config_file(FILE* f, GameConfig* cfg) {
    char line[256];
    char section[64] = "";
    
    while (fgets(line, sizeof(line), f)) {
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') continue;
        
        // Check for section header
        if (line[0] == '[') {
            sscanf(line, "[%[^]]", section);
            continue;
        }
        
        // Parse key=value
        char key[64], value[128];
        if (sscanf(line, "%[^=]=%s", key, value) == 2) {
            apply_config_value(cfg, section, key, value);
        }
    }
}

void save_config(GameConfig* cfg, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return;
    
    fprintf(f, "[Graphics]\n");
    fprintf(f, "width=%d\n", cfg->window_width);
    fprintf(f, "height=%d\n", cfg->window_height);
    fprintf(f, "fullscreen=%s\n", cfg->fullscreen ? "true" : "false");
    fprintf(f, "vsync=%s\n", cfg->vsync ? "true" : "false");
    fprintf(f, "ui_scale=%.1f\n", cfg->ui_scale);
    
    fprintf(f, "\n[Fonts]\n");
    fprintf(f, "card_title_font=%s\n", cfg->card_title_font);
    fprintf(f, "card_stats_font=%s\n", cfg->card_stats_font);
    fprintf(f, "ui_font=%s\n", cfg->ui_font);
    
    // ... etc
    
    fclose(f);
}
```

---

## Phase 8: Desktop Enhancements

### 8.1 Context Menu (Right-Click)

```c
// context_menu.h

typedef struct {
    bool visible;
    int x, y;
    Card* target_card;
    int num_options;
    char options[8][32];
    void (*callbacks[8])(Card*);
} ContextMenu;

void show_context_menu(ContextMenu* menu, Card* card, int x, int y);
void hide_context_menu(ContextMenu* menu);
void render_context_menu(SDL_Renderer* r, ContextMenu* menu, 
                        FontManager* fm);
void handle_context_menu_click(ContextMenu* menu, int x, int y);
```

```c
// context_menu.c

void show_context_menu(ContextMenu* menu, Card* card, 
                       int x, int y) {
    menu->visible = true;
    menu->x = x;
    menu->y = y;
    menu->target_card = card;
    menu->num_options = 0;
    
    // Add relevant options based on card and game state
    add_menu_option(menu, "View Details", show_card_details);
    add_menu_option(menu, "Play Card", play_card);
    if (card->data.is_champion) {
        add_menu_option(menu, "Recall", recall_champion);
    }
}

void add_menu_option(ContextMenu* menu, const char* text,
                     void (*callback)(Card*)) {
    if (menu->num_options >= 8) return;
    strncpy(menu->options[menu->num_options], text, 31);
    menu->callbacks[menu->num_options] = callback;
    menu->num_options++;
}

void render_context_menu(SDL_Renderer* r, ContextMenu* menu,
                        FontManager* fm) {
    if (!menu->visible) return;
    
    int w = 200;
    int h = menu->num_options * 30 + 10;
    
    // Background
    SDL_FRect bg = {menu->x, menu->y, w, h};
    SDL_SetRenderDrawColor(r, 40, 40, 40, 240);
    SDL_RenderFillRect(r, &bg);
    
    // Border
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderRect(r, &bg);
    
    // Options
    for (int i = 0; i < menu->num_options; i++) {
        int y = menu->y + 5 + i * 30;
        draw_text(r, fm->fonts[FONT_UI_NORMAL],
                 menu->options[i], menu->x + 10, y,
                 (SDL_Color){255, 255, 255, 255});
    }
}

void handle_context_menu_click(ContextMenu* menu, int x, int y) {
    if (!menu->visible) return;
    
    int option_height = 30;
    int clicked_option = (y - menu->y - 5) / option_height;
    
    if (clicked_option >= 0 && clicked_option < menu->num_options) {
        menu->callbacks[clicked_option](menu->target_card);
        hide_context_menu(menu);
    }
}
```

### 8.2 Tooltips (Hover)

```c
// tooltip.h

typedef struct {
    bool visible;
    float delay_timer;
    int x, y;
    char text[256];
} Tooltip;

void update_tooltip(Tooltip* tt, float dt, int mouse_x, int mouse_y,
                   Card* hovered_card);
void render_tooltip(SDL_Renderer* r, Tooltip* tt, FontManager* fm);
void format_tooltip_text(Tooltip* tt, Card* card);
```

```c
// tooltip.c

void update_tooltip(Tooltip* tt, float dt, int mouse_x, 
                    int mouse_y, Card* hovered_card) {
    if (hovered_card) {
        tt->delay_timer += dt;
        if (tt->delay_timer > 0.5f) {  // 500ms delay
            tt->visible = true;
            tt->x = mouse_x + 15;
            tt->y = mouse_y + 15;
            format_tooltip_text(tt, hovered_card);
        }
    } else {
        tt->visible = false;
        tt->delay_timer = 0.0f;
    }
}

void format_tooltip_text(Tooltip* tt, Card* card) {
    snprintf(tt->text, 256,
             "%s\n"
             "Species: %s | Rank: %c\n"
             "Attack: d%d+%d | Defense: d%d\n"
             "Cost: %d lunas",
             card->data.name,
             get_species_name(card->data.species),
             'A' + card->data.rank,
             card->data.defense_dice,
             card->data.attack_base,
             card->data.defense_dice,
             card->data.cost);
}

void render_tooltip(SDL_Renderer* r, Tooltip* tt, 
                    FontManager* fm) {
    if (!tt->visible) return;
    
    // Background with padding
    int padding = 10;
    int line_height = 20;
    int num_lines = 4;  // Adjust based on actual content
    int width = 250;
    int height = num_lines * line_height + padding * 2;
    
    SDL_FRect bg = {tt->x, tt->y, width, height};
    SDL_SetRenderDrawColor(r, 30, 30, 30, 240);
    SDL_RenderFillRect(r, &bg);
    
    SDL_SetRenderDrawColor(r, 200, 200, 100, 255);
    SDL_RenderRect(r, &bg);
    
    // Render multi-line text
    char* line = strtok(tt->text, "\n");
    int line_num = 0;
    
    while (line) {
        draw_text(r, fm->fonts[FONT_UI_SMALL], line,
                 tt->x + padding, 
                 tt->y + padding + line_num * line_height,
                 (SDL_Color){255, 255, 200, 255});
        line = strtok(NULL, "\n");
        line_num++;
    }
}
```

### 8.3 Keyboard Hint Overlay

```c
// ui_overlay.c

typedef struct {
    bool visible;
    float fade_timer;
} ShortcutHint;

void render_keyboard_hints(SDL_Renderer* r, FontManager* fm,
                           GameState* g) {
    if (!g->show_hints) return;
    
    SDL_Color hint_color = {255, 255, 150, 180};
    TTF_Font* font = fm->fonts[FONT_UI_SMALL];
    
    int x = 10, y = 10;
    
    // Show hints for available actions based on game phase
    if (g->phase == PHASE_PLAYER_TURN) {
        draw_text(r, font, "[1-9] Select Card", x, y, hint_color);
        y += 20;
        draw_text(r, font, "[SPACE] Play Cards", x, y, hint_color);
        y += 20;
        draw_text(r, font, "[ENTER] Pass Turn", x, y, hint_color);
        y += 20;
    }
    
    // Always available
    draw_text(r, font, "[ESC] Cancel", x, y, hint_color);
    y += 20;
    draw_text(r, font, "[TAB] Next Card", x, y, hint_color);
    y += 20;
    draw_text(r, font, "[F1] Toggle Hints", x, y, hint_color);
}

void toggle_hints(GameState* g) {
    g->show_hints = !g->show_hints;
}
```

### 8.4 Card Selection Visual Feedback

```c
// When user presses 1-9, highlight corresponding card

void highlight_card_at_index(GameState* g, int index) {
    if (index < 0 || index >= g->hand_size) return;
    
    // Clear previous selections if Ctrl not held
    if (!is_ctrl_held()) {
        clear_all_selections(g);
    }
    
    g->hand[index].is_selected = !g->hand[index].is_selected;
    
    // Play subtle sound effect
    play_sfx(SFX_CARD_SELECT);
}

bool is_ctrl_held(void) {
    const uint8_t* state = SDL_GetKeyboardState(NULL);
    return state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
}
```

---

## Phase 9: Platform-Specific Considerations

### 9.1 Windows/Linux Desktop

**Pros:**
- Easy development and testing
- Full debugging tools
- Fast iteration

**Specific needs:**
- **High DPI awareness**: Handle Windows display scaling
- **Multiple screen support**: Remember window position
- **File paths**: Use `SDL_GetBasePath()` for cross-platform asset loading

```c
// platform_windows.c / platform_linux.c

void init_platform(void) {
    #ifdef _WIN32
        // Windows-specific: Enable high DPI awareness
        SetProcessDPIAware();
    #endif
    
    // Get base path for assets
    char* base_path = SDL_GetBasePath();
    if (base_path) {
        printf("Base path: %s\n", base_path);
        SDL_free(base_path);
    }
}

float get_dpi_scale(SDL_Window* window) {
    int window_w, window_h;
    int drawable_w, drawable_h;
    
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_GetWindowSizeInPixels(window, &drawable_w, &drawable_h);
    
    return (float)drawable_w / (float)window_w;
}
```

### 9.2 iOS Specifics

**Challenges:**
- Apple Developer account required ($99/year)
- Code signing complexity
- Must use Xcode for final build

**Architecture:**
```
iOS App
├── Objective-C/Swift wrapper (minimal)
└── oracle_core.a (your C game compiled as static library)
```

**Key considerations:**
- No file system access outside sandbox
- Bundle assets in app package
- Handle iPad multitasking
- Support portrait and landscape
- TestFlight for beta testing

```c
// platform_ios.m

void ios_get_document_path(char* buffer, size_t size) {
    NSArray* paths = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES);
    NSString* documentsDirectory = [paths objectAtIndex:0];
    strncpy(buffer, [documentsDirectory UTF8String], size);
}

void ios_handle_lifecycle_event(AppLifecycleEvent event) {
    switch(event) {
        case APP_WILL_RESIGN_ACTIVE:
            // Save game state
            save_game_state();
            break;
        case APP_DID_BECOME_ACTIVE:
            // Resume game
            resume_game();
            break;
        case APP_WILL_TERMINATE:
            // Final cleanup
            cleanup_all();
            break;
    }
}
```

### 9.3 Android Specifics

**Challenges:**
- Device fragmentation
- Various screen densities

**Architecture:**
```
Android App
├── Java/Kotlin wrapper (minimal)
└── liboracle.so (your C game compiled as shared library)
```

**Key considerations:**
- Test on multiple virtual devices
- Handle hardware back button
- Request storage permissions if saving state
- Google Play Console for distribution

```c
// platform_android.c

void android_get_external_storage(char* buffer, size_t size) {
    // Access via JNI
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    
    // Call Java method to get external storage path
    // ... JNI code ...
}

void android_handle_back_button(void) {
    // Show confirmation dialog or go to previous screen
    if (current_screen == SCREEN_GAME) {
        show_pause_menu();
    } else {
        go_back_one_screen();
    }
}

void android_log(const char* tag, const char* message) {
    __android_log_print(ANDROID_LOG_INFO, tag, "%s", message);
}
```

### 9.4 Debug Logging Abstraction

```c
// logger.h

#ifdef __ANDROID__
    #include <android/log.h>
    #define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, \
                                             "Oracle", __VA_ARGS__)
    #define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, \
                                              "Oracle", __VA_ARGS__)
#elif defined(__APPLE__)
    #include <os/log.h>
    #define LOG_INFO(...) os_log(OS_LOG_DEFAULT, __VA_ARGS__)
    #define LOG_ERROR(...) os_log_error(OS_LOG_DEFAULT, __VA_ARGS__)
#else
    #define LOG_INFO(...) printf(__VA_ARGS__)
    #define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#endif
```

---

## Phase 10: Development Roadmap

### Milestone 1: Desktop Prototype (5-7 weeks)

**Week 1-2: Core Foundation**
- SDL3 + SDL_ttf + SDL_image setup on Windows (MSYS2) and Linux
- Window, renderer, event loop
- Font manager implementation
- Load and display single card with text overlay

**Week 3-4: Input & Interaction**
- Mouse input (click, hover, drag)
- Keyboard shortcut system
- Card selection visual feedback
- Context menu prototype
- Tooltip system

**Week 5-6: Card Rendering**
- Complete card compositor matching your design
- Hand visualization (9 cards)
- Battle zone layout (3v3 cards)
- Basic animations (card slide, selection pulse)

**Week 7: UI Polish**
- Tooltips on hover with full card info
- Keyboard hint overlay
- Energy/luna display (top bars)
- Main menu screen
- Game state visualization

**Deliverable**: Playable desktop prototype with keyboard + mouse control

---

### Milestone 2: Touch Input & Tablet Layout (3-4 weeks)

**Week 8-9: Touch Abstraction**
- Touch input handling
- Gesture recognition (tap, long-press, drag)
- Touch-friendly button sizing (48dp minimum)
- Pinch-to-zoom for card details (optional)

**Week 10-11: Responsive Layout**
- Layout system using normalized coordinates
- Tablet UI (based on mockup)
- Orientation support (landscape primary)
- DPI scaling for different tablets

**Deliverable**: Desktop version with touch support simulation

---

### Milestone 3: Mobile Platform Ports (6-8 weeks)

**iOS Port (Weeks 12-15):**
- Set up Xcode project with SDL3
- Create iOS app wrapper
- Link C game core as static library
- Test on iPad simulator
- Handle iOS lifecycle (background/foreground)
- Optimize for Metal rendering
- App Store metadata preparation

**Android Port (Weeks 16-19):**
- Set up Android Studio with NDK
- Use SDL3's Android template
- JNI bridge (minimal - SDL3 handles most)
- Test on Android emulator (Pixel Tablet profile)
- Handle Android lifecycle
- Optimize for Vulkan rendering
- Play Store metadata preparation

**Deliverable**: Functional iOS and Android tablet builds

---

### Milestone 4: Content & Polish (4-6 weeks)

**Week 20-22: Asset Integration**
- Integrate all 102 champion artworks
- Add all 15 species icons
- Add 5 order symbols
- Sound effects and music
- Animation polish

**Week 23-25: Testing & Optimization**
- Performance profiling
- Memory leak detection
- Cross-platform testing
- Bug fixes
- User testing feedback

**Deliverable**: Release candidate builds for all platforms

---

## Code Examples

### Complete Minimal Card Rendering Demo

```c
// minimal_card_demo.c
// Demonstrates SDL3 card rendering with text overlay

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <math.h>

typedef struct {
    char name[32];
    int attack, defense, cost;
    uint8_t defense_dice;
    SDL_Texture* artwork;
    bool selected;
} Card;

void render_card(SDL_Renderer* r, Card* c, TTF_Font* font,
                 int x, int y, int w, int h) {
    // Border (colored based on card type)
    SDL_SetRenderDrawColor(r, 75, 0, 130, 255);  // Indigo
    SDL_FRect border = {x, y, w, h};
    SDL_RenderFillRect(r, &border);
    
    // Inner background
    SDL_SetRenderDrawColor(r, 245, 245, 245, 255);
    SDL_FRect inner = {x+6, y+6, w-12, h-12};
    SDL_RenderFillRect(r, &inner);
    
    // Artwork area
    if (c->artwork) {
        SDL_FRect art = {x+70, y+50, w-86, h-120};
        SDL_RenderTexture(r, c->artwork, NULL, &art);
    }
    
    // Left sidebar stats
    SDL_Color black = {0, 0, 0, 255};
    char dice_str[8];
    snprintf(dice_str, 8, "d%d", c->defense_dice);
    
    SDL_Surface* surf = TTF_RenderText_Blended(font, dice_str, black);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        SDL_FRect dst = {x+15, y+40, surf->w, surf->h};
        SDL_RenderTexture(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_DestroySurface(surf);
    }
    
    // Name banner
    SDL_SetRenderDrawColor(r, 245, 245, 220, 255);
    SDL_FRect banner = {x+6, y+h-60, w-12, 50};
    SDL_RenderFillRect(r, &banner);
    
    surf = TTF_RenderText_Blended(font, c->name, black);
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
        int tx = x + (w - surf->w) / 2;
        SDL_FRect dst = {tx, y+h-40, surf->w, surf->h};
        SDL_RenderTexture(r, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_DestroySurface(surf);
    }
    
    // Selection indicator
    if (c->selected) {
        static float pulse = 0.0f;
        pulse += 0.05f;
        float alpha = 150 + 105 * sinf(pulse);
        SDL_SetRenderDrawColor(r, 255, 255, 100, (int)alpha);
        for (int i = 0; i < 3; i++) {
            SDL_FRect sel = {x-i, y-i, w+i*2, h+i*2};
            SDL_RenderRect(r, &sel);
        }
    }
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    
    SDL_Window* win = SDL_CreateWindow("Oracle Card Demo",
        1280, 720, 0);
    SDL_Renderer* r = SDL_CreateRenderer(win, NULL);
    
    TTF_Font* font = TTF_OpenFont("assets/fonts/title.ttf", 20);
    if (!font) {
        printf("Failed to load font\n");
        return 1;
    }
    
    Card card = {
        .name = "Lof",
        .attack = 0,
        .defense = 2,
        .cost = 0,
        .defense_dice = 4,
        .artwork = IMG_LoadTexture(r, "assets/cards/artwork/lof.png"),
        .selected = false
    };
    
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                if (e.key.key == SDLK_1) card.selected = !card.selected;
                if (e.key.key == SDLK_ESCAPE) running = false;
            }
        }
        
        SDL_SetRenderDrawColor(r, 30, 30, 30, 255);
        SDL_RenderClear(r);
        
        render_card(r, &card, font, 100, 50, 280, 370);
        
        SDL_RenderPresent(r);
        SDL_Delay(16);  // ~60 FPS
    }
    
    if (card.artwork) SDL_DestroyTexture(card.artwork);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(win);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    
    return 0;
}
```

**Compile:**
```bash
# Linux
gcc minimal_card_demo.c -o demo \
    `pkg-config --cflags --libs sdl3 SDL3_image SDL3_ttf` -lm

# Windows (MSYS2)
gcc minimal_card_demo.c -o demo.exe \
    -lSDL3 -lSDL3_image -lSDL3_ttf -lm
```

---

## Order Symbol Design

### Recommended Order Symbols

Based on the thematic names of each Order, here are simple, distinct symbols that work well at small sizes:

#### **Order A - Dawn Light / Lumière d'Aurore**
**Species**: Human, Elf, Dwarf

- **Symbol**: **Rising sun** (semicircle with rays pointing upward)
- **Color**: Golden yellow (#FFD700) or warm orange (#FFA500)
- **Meaning**: Dawn represents new beginnings, civilization, and enlightenment
- **Simple design**: Half-circle at bottom with 5-7 triangular rays

#### **Order B - Verdant Light / Lumière Verdoyante**
**Species**: Hobbit, Faun, Centaur

- **Symbol**: **Leaf** or **simple tree** (three-pointed leaf or Y-shaped tree)
- **Color**: Forest green (#228B22) or emerald (#50C878)
- **Meaning**: Verdant = lush/green vegetation, nature-dwellers, harmony with earth
- **Simple design**: Three-pointed maple leaf or stylized tree with three branches

#### **Order C - Ember Light / Lumière de Braise**
**Species**: Orc, Goblin, Minotaur

- **Symbol**: **Flame** (simple triangular flame shape)
- **Color**: Bright red (#DC143C) or orange-red (#FF4500)
- **Meaning**: Ember = glowing coal/fire, represents passion, intensity, and raw power
- **Simple design**: Teardrop or triangle pointing upward with wavy top edge

#### **Order D - Eternal Light / Lumière Éternelle**
**Species**: Dragon, Cyclops, Fairy

- **Symbol**: **Star** (4 or 5-pointed star) or **diamond**
- **Color**: Brilliant white (#FFFFFF), silver (#C0C0C0), or cyan blue (#00BFFF)
- **Meaning**: Eternal = timeless/cosmic, ancient and powerful mythical beings
- **Simple design**: Four-pointed star (like a compass rose) or diamond shape

#### **Order E - Moonlight / Lumière Lunaire**
**Species**: Aven, Koatl, Lycan

- **Symbol**: **Crescent moon** (facing right)
- **Color**: Pale blue (#B0E0E6), silver-white (#E8E8E8), or soft purple (#9370DB)
- **Meaning**: Direct lunar connection, creatures of night/sky, mystery and instinct
- **Simple design**: Classic crescent moon shape

### Visual Summary

```
Order A: ☀️  Rising Sun      - Gold/Yellow     (Human, Elf, Dwarf)
Order B: 🍃  Leaf            - Green           (Hobbit, Faun, Centaur)
Order C: 🔥  Flame           - Red/Orange      (Orc, Goblin, Minotaur)
Order D: ⭐  Star/Diamond    - White/Cyan      (Dragon, Cyclops, Fairy)
Order E: 🌙  Crescent Moon   - Silver/Blue     (Aven, Koatl, Lycan)
```

### Code Implementation

```c
// order_symbols.h

typedef enum {
    ORDER_SYMBOL_SUN,        // Order A
    ORDER_SYMBOL_LEAF,       // Order B
    ORDER_SYMBOL_FLAME,      // Order C
    ORDER_SYMBOL_STAR,       // Order D
    ORDER_SYMBOL_CRESCENT    // Order E
} OrderSymbol;

// Colors for each order
const SDL_Color ORDER_COLORS[5] = {
    {255, 215, 0, 255},      // A: Gold
    {34, 139, 34, 255},      // B: Forest Green
    {220, 20, 60, 255},      // C: Crimson Red
    {0, 191, 255, 255},      // D: Deep Sky Blue
    {192, 192, 192, 255}     // E: Silver
};

const char* ORDER_SYMBOL_PATHS[5] = {
    "assets/icons/orders/order_a_sun.png",
    "assets/icons/orders/order_b_leaf.png",
    "assets/icons/orders/order_c_flame.png",
    "assets/icons/orders/order_d_star.png",
    "assets/icons/orders/order_e_crescent.png"
};
```

### Alternative: Geometric Shapes

If you prefer more abstract/geometric symbols:

| Order | Shape | Meaning | Color |
|-------|-------|---------|-------|
| A | **Triangle pointing up** | Dawn/ascension | Gold |
| B | **Square** | Earth/stability | Green |
| C | **Inverted triangle** | Fire element | Red |
| D | **Circle** | Eternity/perfection | White/Cyan |
| E | **Crescent** | Moon phases | Silver |

### Python Script to Generate Simple Order Icons

```python
# tools/generate_order_icons.py

from PIL import Image, ImageDraw
import math

def create_sun_icon(size=64):
    """Order A: Rising Sun"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    center_x, center_y = size // 2, size * 3 // 4
    radius = size // 4
    
    # Draw rays
    for i in range(8):
        angle = math.pi * (i / 8.0 + 1.0)
        x1 = center_x + radius * math.cos(angle)
        y1 = center_y + radius * math.sin(angle)
        x2 = center_x + (radius + 10) * math.cos(angle)
        y2 = center_y + (radius + 10) * math.sin(angle)
        draw.line([(x1, y1), (x2, y2)], fill=(255, 215, 0, 255), width=3)
    
    # Draw semicircle
    bbox = [center_x - radius, center_y - radius, 
            center_x + radius, center_y + radius]
    draw.pieslice(bbox, 180, 360, fill=(255, 215, 0, 255))
    
    return img

def create_leaf_icon(size=64):
    """Order B: Leaf"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Simple three-pointed leaf shape
    points = [
        (size//2, size//8),           # Top point
        (size*3//4, size//2),         # Right point
        (size//2, size*7//8),         # Bottom center
        (size//4, size//2),           # Left point
    ]
    draw.polygon(points, fill=(34, 139, 34, 255))
    
    # Center vein
    draw.line([(size//2, size//8), (size//2, size*7//8)], 
             fill=(20, 100, 20, 255), width=2)
    
    return img

def create_flame_icon(size=64):
    """Order C: Flame"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Teardrop/flame shape
    points = [
        (size//2, size//8),           # Top point
        (size*5//8, size//2),         # Right curve
        (size*9//16, size*3//4),      # Right bottom
        (size//2, size*7//8),         # Bottom point
        (size*7//16, size*3//4),      # Left bottom
        (size*3//8, size//2),         # Left curve
    ]
    draw.polygon(points, fill=(220, 20, 60, 255))
    
    # Inner flame
    inner_points = [
        (size//2, size//4),
        (size*9//16, size//2),
        (size//2, size*3//4),
        (size*7//16, size//2),
    ]
    draw.polygon(inner_points, fill=(255, 140, 0, 255))
    
    return img

def create_star_icon(size=64):
    """Order D: Four-pointed star"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    center = size // 2
    outer_radius = size * 3 // 8
    inner_radius = size // 6
    
    points = []
    for i in range(4):
        angle = math.pi / 2 * i
        # Outer point
        x = center + outer_radius * math.cos(angle)
        y = center + outer_radius * math.sin(angle)
        points.append((x, y))
        # Inner point
        angle += math.pi / 4
        x = center + inner_radius * math.cos(angle)
        y = center + inner_radius * math.sin(angle)
        points.append((x, y))
    
    draw.polygon(points, fill=(0, 191, 255, 255))
    
    return img

def create_crescent_icon(size=64):
    """Order E: Crescent Moon"""
    img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    center_x, center_y = size // 2, size // 2
    outer_radius = size * 3 // 8
    
    # Draw full circle
    bbox1 = [center_x - outer_radius, center_y - outer_radius,
             center_x + outer_radius, center_y + outer_radius]
    draw.ellipse(bbox1, fill=(192, 192, 192, 255))
    
    # Cut out inner circle to create crescent
    offset = outer_radius // 2
    bbox2 = [center_x - outer_radius + offset, center_y - outer_radius,
             center_x + outer_radius + offset, center_y + outer_radius]
    draw.ellipse(bbox2, fill=(0, 0, 0, 0))
    
    return img

def generate_all_order_icons():
    """Generate all 5 order symbol icons"""
    import os
    os.makedirs('assets/icons/orders', exist_ok=True)
    
    icons = [
        ('order_a_sun.png', create_sun_icon),
        ('order_b_leaf.png', create_leaf_icon),
        ('order_c_flame.png', create_flame_icon),
        ('order_d_star.png', create_star_icon),
        ('order_e_crescent.png', create_crescent_icon),
    ]
    
    for filename, create_func in icons:
        img = create_func(64)
        path = f'assets/icons/orders/{filename}'
        img.save(path)
        print(f"Generated: {path}")
        
        # Also generate @2x version
        img_2x = create_func(128)
        path_2x = f'assets/icons/orders/{filename.replace(".png", "@2x.png")}'
        img_2x.save(path_2x)
        print(f"Generated: {path_2x}")

if __name__ == '__main__':
    generate_all_order_icons()
```

### Rendering Order Symbols on Cards

```c
// In your card rendering code

void render_order_symbol(SDL_Renderer* r, TextureCache* tc,
                        ChampionOrder order, int x, int y, int size) {
    const char* symbol_path = ORDER_SYMBOL_PATHS[order];
    SDL_Texture* tex = get_texture(tc, symbol_path);
    
    if (!tex) return;
    
    SDL_FRect dst = {x, y, size, size};
    
    // Optional: tint with order color
    SDL_Color color = ORDER_COLORS[order];
    SDL_SetTextureColorMod(tex, color.r, color.g, color.b);
    
    SDL_RenderTexture(r, tex, NULL, &dst);
}
```

### Benefits of Thematic Symbols vs. Geometric

**Thematic (Sun, Leaf, Flame, Star, Crescent):**
- ✅ More evocative and memorable
- ✅ Instantly convey the "flavor" of each Order
- ✅ Help players associate species with their Order
- ✅ Better for storytelling and world-building
- ❌ Slightly more complex to draw at very small sizes

**Geometric (Triangle, Square, Circle, etc.):**
- ✅ Very simple to render and recognize
- ✅ Clear at any size
- ✅ Culturally neutral
- ❌ Less thematic connection to the game world
- ❌ More abstract, harder to remember meaning

**Recommendation**: Use the **thematic symbols** (sun, leaf, flame, star, crescent) as they enhance the game's fantasy theme and make the Orders more distinctive and memorable.

---

## Additional Development Tips

### Performance Optimization

```c
// Batch rendering for multiple cards
void render_card_batch(SDL_Renderer* r, Card* cards, int count,
                       FontManager* fm, TextureCache* tc) {
    // Pre-cache all needed textures
    for (int i = 0; i < count; i++) {
        get_texture(tc, cards[i].data.artwork_file);
        get_texture(tc, cards[i].data.species_icon_path);
    }
    
    // Render all cards
    for (int i = 0; i < count; i++) {
        int x = calculate_card_x(i);
        int y = calculate_card_y(i);
        render_champion_card_complete(r, &cards[i], x, y, 
                                     CARD_WIDTH, CARD_HEIGHT,
                                     fm, tc);
    }
}
```

### Animation System

```c
// Simple animation system for card movements
typedef struct {
    Card* card;
    float start_x, start_y;
    float end_x, end_y;
    float progress;  // 0.0 to 1.0
    float duration;
} CardAnimation;

void update_animations(CardAnimation* anims, int count, float dt) {
    for (int i = 0; i < count; i++) {
        if (anims[i].progress < 1.0f) {
            anims[i].progress += dt / anims[i].duration;
            if (anims[i].progress > 1.0f) {
                anims[i].progress = 1.0f;
            }
        }
    }
}

void get_animated_position(CardAnimation* anim, float* x, float* y) {
    // Ease-out cubic
    float t = anim->progress;
    float ease = 1.0f - powf(1.0f - t, 3.0f);
    
    *x = anim->start_x + (anim->end_x - anim->start_x) * ease;
    *y = anim->start_y + (anim->end_y - anim->start_y) * ease;
}
```

### Memory Management

```c
// Resource pool for frequently used objects
typedef struct {
    Card* cards;
    int capacity;
    int used;
} CardPool;

CardPool* create_card_pool(int capacity) {
    CardPool* pool = malloc(sizeof(CardPool));
    pool->cards = malloc(sizeof(Card) * capacity);
    pool->capacity = capacity;
    pool->used = 0;
    return pool;
}

Card* acquire_card(CardPool* pool) {
    if (pool->used >= pool->capacity) return NULL;
    return &pool->cards[pool->used++];
}

void release_card(CardPool* pool, Card* card) {
    // Mark card as available for reuse
    // Implementation depends on your pooling strategy
}
```

### Testing Utilities

```c
// Debug overlay for development
void render_debug_info(SDL_Renderer* r, FontManager* fm,
                       GameState* g, float fps) {
    char debug_text[256];
    
    snprintf(debug_text, 256,
             "FPS: %.1f | Cards in hand: %d | Energy: %d/%d",
             fps, g->hand_size, 
             g->current_energy[PLAYER_A],
             g->current_energy[PLAYER_B]);
    
    draw_text(r, fm->fonts[FONT_UI_SMALL], debug_text,
             10, 10, (SDL_Color){255, 255, 0, 255});
}

// Screenshot functionality
void take_screenshot(SDL_Renderer* r, const char* filename) {
    int w, h;
    SDL_GetRenderOutputSize(r, &w, &h);
    
    SDL_Surface* surface = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    SDL_RenderReadPixels(r, NULL, surface->format, surface->pixels, 
                        surface->pitch);
    
    IMG_SavePNG(surface, filename);
    SDL_DestroySurface(surface);
    
    printf("Screenshot saved: %s\n", filename);
}
```

---

## Summary Checklist

### Phase 1 ✓ Architecture
- [ ] Directory structure created
- [ ] Build system chosen (Makefile/CMake)
- [ ] SDL3 installed on development platforms

### Phase 2 ✓ Core Systems
- [ ] Window and renderer initialized
- [ ] Event loop implemented
- [ ] Input abstraction layer complete
- [ ] Keyboard shortcuts working

### Phase 3 ✓ Rendering
- [ ] Font manager implemented
- [ ] Texture cache working
- [ ] Card rendering matches design spec
- [ ] UI elements render correctly

### Phase 4 ✓ Assets
- [ ] All 102 champion names mapped to fullDeck
- [ ] Species icons (15) created/acquired
- [ ] Order symbols (5) designed
- [ ] Font files acquired and licensed

### Phase 5 ✓ Desktop Features
- [ ] Context menu functional
- [ ] Tooltips working
- [ ] Keyboard hints overlay
- [ ] Configuration system

### Phase 6 ✓ Polish
- [ ] Animations smooth
- [ ] Performance optimized
- [ ] Sound effects integrated
- [ ] All game states functional

### Phase 7 ✓ Mobile
- [ ] Touch input working
- [ ] iOS build functional
- [ ] Android build functional
- [ ] Tablet layouts tested

---

## Resources

### SDL3 Documentation
- Official docs: https://wiki.libsdl.org/SDL3/
- Migration guide: https://github.com/libsdl-org/SDL/blob/main/docs/README-migration.md

### Font Resources
- Google Fonts: https://fonts.google.com
- Font licensing guide: https://fonts.google.com/about

### Platform-Specific
- iOS with SDL: https://wiki.libsdl.org/SDL3/README/ios
- Android with SDL: https://wiki.libsdl.org/SDL3/README/android

### Tools
- GIMP (image editing): https://www.gimp.org/
- Inkscape (vector graphics): https://inkscape.org/
- Pillow (Python image library): https://python-pillow.org/

---

## End of Document

This comprehensive plan covers all aspects of developing the Oracle card game GUI with SDL3 across Windows, Linux, iOS, and Android platforms. The modular architecture ensures code remains maintainable within the 30-line function and 500-line file constraints.

**Next steps**: Begin with Milestone 1 (Desktop Prototype) and implement one module at a time, testing thoroughly before moving to the next component.