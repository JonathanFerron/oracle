/* ============================================================
   localization.h - Localization support for UI strings
   ============================================================ */

#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#include "game_types.h"

/* Macro for context-sensitive localized strings */
#define LOCALIZED_STRING(en, fr, es) \
  ((const char*[]){en, fr, es}[cfg->language])

/* Helper function for when cfg is not directly accessible */
#define LOCALIZED_STRING_L(lang, en, fr, es) \
  ((const char*[]){en, fr, es}[lang])

#endif /* LOCALIZATION_H */
