// gui_combat_panel.h
// Draws one revealed combat (gui_reveal.h) into the board: each side's champion
// cards in its combat zone, a die per champion in that seat's dice row (the
// rolled number in an n-gon, attackers also "+base = total"), and beside the
// cards the side's combo bonus and total -- plus, once fully revealed, the
// damage and the defender's energy before -> after. Design: plan file's
// "Next up" item 2 (Jonathan, 2026-09-28/29).

#ifndef GUI_COMBAT_PANEL_H
#define GUI_COMBAT_PANEL_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "../../core/combat.h"
#include "../../core/game_types.h"
#include "gui_layout.h"
#include "gui_reveal.h" // RevealStage
#include "../../visibility/visible_state.h"

// `attacker` is who attacked (seat mapping follows `viewer`). In REVEAL_ATTACK
// only the attacker's dice/total are shown and the defender's dice are empty
// n-gons; REVEAL_FULL shows everything. `text_font` is the status font.
void gui_combat_panel_draw(SDL_Renderer* renderer, TTF_Font* card_font, TTF_Font* text_font,
                           const GuiLayout* layout, const CombatDetails* d, PlayerID attacker,
                           PlayerID viewer, RevealStage stage, ui_language_t lang);

// The pre-roll preview: an empty n-gon (right shape and colour, no number) for
// every champion committed in a combat zone -- e.g. the attacker's, on a defense
// decision -- and for the viewer's own not-yet-confirmed `staged` champions
// (fullDeck indices, shown where their cards will land).
void gui_combat_panel_draw_preview(SDL_Renderer* renderer, const GuiLayout* layout,
                                   const VisibleGameState* view, const uint8_t* staged,
                                   uint8_t staged_count);

#endif // GUI_COMBAT_PANEL_H
