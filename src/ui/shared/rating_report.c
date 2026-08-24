// rating_report.c
// See rating_report.h.

#include "rating_report.h"
#include "player_config.h"
#include "localization.h"
#include <stdio.h>

// Selection sort by rating descending into `order`. Deliberately avoids
// the v2 spec's `for(i = 0; i < num_entrants - 1; i++)` pattern, which
// underflows (unsigned) on an empty roster.
static void sorted_order(const RatingSystem* rs, uint32_t* order)
{ for(uint32_t i = 0; i < rs->num_entrants; i++) order[i] = i;

  for(uint32_t i = 0; i + 1 < rs->num_entrants; i++)
  { uint32_t best = i;
    for(uint32_t j = i + 1; j < rs->num_entrants; j++)
      if(rs->entrants[order[j]].rating > rs->entrants[order[best]].rating) best = j;

    uint32_t tmp = order[i];
    order[i] = order[best];
    order[best] = tmp;
  }
} // sorted_order

static const char* entrant_label(const RatingEntry* e, ui_language_t lang)
{ if(e->kind == RATING_ENTRANT_AI) return get_strategy_display_name(e->strategy, lang);
  return e->name;
} // entrant_label

static void print_leaderboard_row(const RatingSystem* rs, uint32_t id, ui_language_t lang)
{ const RatingEntry* e = &rs->entrants[id];
  double wr = e->games_played > 0 ? 100.0 * e->games_won / e->games_played : 0.0;
  const char* anchor_mark = (id == rs->borealis_id) ? " *" : "";

  printf("%-24s %6d %8u %6u %6.1f%%%s\n",
         entrant_label(e, lang), e->rating, e->games_played, e->games_won, wr,
         anchor_mark);
} // print_leaderboard_row

void rating_print_leaderboard(const RatingSystem* rs, ui_language_t lang)
{ printf("\n%s\n", LOCALIZED_STRING_L(lang, "=== Oracle Rating Leaderboard ===",
                                      "=== Classement Oracle ===",
                                      "=== Clasificacion Oracle ==="));

  if(rs->num_entrants == 0)
  { printf("%s\n", LOCALIZED_STRING_L(lang, "(no entrants)", "(aucun participant)",
                                      "(sin participantes)"));
    return;
  }

  printf("%-24s %6s %8s %6s %7s\n", "Entrant", "Rating", "Games", "Wins", "WinRate");
  printf("-------------------------------------------------------\n");

  uint32_t order[RATING_MAX_ENTRANTS];
  sorted_order(rs, order);
  for(uint32_t i = 0; i < rs->num_entrants; i++)
    print_leaderboard_row(rs, order[i], lang);

  printf("%s\n", LOCALIZED_STRING_L(lang, "(* = Borealis, the rating-50 anchor)",
                                    "(* = Borealis, l'ancre a la cote 50)",
                                    "(* = Borealis, el ancla en la puntuacion 50)"));
} // rating_print_leaderboard

void rating_print_entrant_details(const RatingSystem* rs, uint32_t id, ui_language_t lang)
{ const RatingEntry* e = rating_get_entrant(rs, id);
  if(!e) return;

  printf("\n%s: %s\n", LOCALIZED_STRING_L(lang, "Entrant", "Participant", "Participante"),
         entrant_label(e, lang));
  printf("%s: %d (%s: %.4f)\n",
         LOCALIZED_STRING_L(lang, "Rating", "Cote", "Puntuacion"), e->rating,
         LOCALIZED_STRING_L(lang, "strength", "force", "fuerza"), e->bt_strength);

  double wr = e->games_played > 0 ? 100.0 * e->games_won / e->games_played : 0.0;
  printf("%s: %u, %s: %u (%.1f%%), %s: %u\n",
         LOCALIZED_STRING_L(lang, "Games", "Parties", "Partidas"), e->games_played,
         LOCALIZED_STRING_L(lang, "Wins", "Victoires", "Victorias"), e->games_won, wr,
         LOCALIZED_STRING_L(lang, "Draws", "Nuls", "Empates"), e->games_drawn);

  double ci = rating_confidence_interval(rs, id, 1.96);
  if(ci >= 0.0)
    printf("95%% %s: +/-%.1f\n", LOCALIZED_STRING_L(lang, "CI", "IC", "IC"), ci);
} // rating_print_entrant_details
