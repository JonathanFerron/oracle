# Oracle TCG — Expansions Design Handout (Final)

**Status:** Approved design decisions from chat session
**Scope:** Two future expansions, 102 champions each (15 species × 3 colors × 5 orders, plus utility cards)
**Purpose:** Context briefing for Claude Code implementation planning — no code should be written from this document directly; use it to produce a scoped implementation handout first.

---

## 1. Design Constraints That Shaped These Decisions

- **"Vampire" is already a champion *ability* name** in the base game (see `ideas/game-depth-additions ideas.md`). No species may be named Vampire.
- **Existing color palette is functionally partitioned**, not just aesthetic:
  - Champion colors (Red, Indigo, Orange) identify a champion's color for deck-building and combo bonuses.
  - Utility colors (Purple = Draw 3/Recall 2, Green = Draw 2/Recall 1) identify *card type*, not champion allegiance.
  - Teal (three shades) and Gray are reserved for UI/branding (logo, card backs, GUI table top, card text) — not gameplay colors at all.
- **Rule derived from the above:** new champion colors must stay clear of the Purple utility zone (~274° ±15°) and Green utility zone (~162° ±15°), and should stay visually distinct from Teal/Gray UI colors.
- All new content must preserve existing combo bonus logic (Species > Order > Color matching) with zero changes to `combo_bonus.c` — new colors/orders/species are pure data additions.

---

## 2. Complete Color Reference (All 9 Champion Colors + Reserved/UI)

### Champion Colors (gameplay, deck-building relevant)

| # | Set | Color Name | Display Name | HSL | Approx Hex | Theme |
|---|-----|-----------|---------------|-----|-------------|-------|
| 1 | Base | Red | Cardinal | (350°, 80%, 40%) | #B3294D | — |
| 2 | Base | Indigo | Midnight | (244°, 80%, 30%) | #0F1A66 | — |
| 3 | Base | Orange | Sunset | (26°, 95%, 40%) | #C65D0D | — |
| 4 | Exp1 | Magenta | Twilight | (310°, 70%, 45%) | #D6149A approx | Shadow magic, dusk realm |
| 5 | Exp1 | Crimson | Ruby | (345°, 85%, 45%) | #D61443 | Blood magic, life force |
| 6 | Exp1 | Bronze | Copper | (30°, 70%, 35%) | #985C1F | Alchemy, metallurgy, earth |
| 7 | Exp2 | Gold | Sunstone | (45°, 100%, 50%) | #FFBF00 | Divine power, radiance, glory |
| 8 | Exp2 | Sky Blue | Sapphire | (210°, 80%, 55%) | #2DC4FF | Sky, wind, storms |
| 9 | Exp2 | Forest Green | Emerald | (140°, 60%, 40%) | #29A366 | Life, growth, vitality |

### Reserved Utility Colors (card-type markers — NEVER used as champion colors)

| Color | Function | HSL |
|-------|----------|-----|
| Purple | Draw 3 Cards / Recall 2 champions | (274°, 53%, 42%) |
| Green (utility) | Draw 2 Cards / Recall 1 champion | (162°, 35%, 35%) |

### UI / Branding Colors (not gameplay-relevant, do not reuse for champions)

| Element | HSL |
|---------|-----|
| Card Back Orange | (22°, 90%, 50%) |
| Card Back Red | (0°, 78%, 50%) |
| Card Back Indigo | (205°, 62%, 42%) |
| Teal Logo Text | (192°, 57%, 30%) |
| Teal Card Back | (186°, 65%, 32%) |
| Teal Table Top (GUI) | (181°, 23%, 60%) |
| Card Text Gray | (0°, 0%, 30%) |

**Hue separation rule of thumb applied:** ≥15° between any two champion colors; ≥30° buffer maintained around each reserved utility hue (162°, 274°).

---

## 3. Expansion 1 — "The Shadow Realms"

**Aesthetic:** Underground civilizations, nocturnal creatures, hidden/liminal worlds. Darker palette, bioluminescence, crystalline structures.

### 3.1 Colors (3)

| Color | Name | HSL | Theme |
|-------|------|-----|-------|
| Magenta | Twilight | (310°, 70%, 45%) | Shadow magic, dusk realm |
| Crimson | Ruby | (345°, 85%, 45%) | Blood magic, life force |
| Bronze | Copper | (30°, 70%, 35%) | Alchemy, metallurgy, earth |

### 3.2 Orders (5)

| Order | EN | FR | ES | Theme | Philosophy |
|-------|----|----|----|-------|------------|
| F | Shadow Light | Lumière d'Ombre | Luz de Sombra | Stealth, subterfuge, hidden knowledge | "True sight sees what lurks unseen" |
| G | Deep Light | Lumière Profonde | Luz Profunda | Underground realms, earth magic, mining | "Strength forged in darkness" |
| H | Crystal Light | Lumière Cristalline | Luz Cristalina | Purity, clarity, hard truths | "Facets reveal inner beauty" |
| I | Tide Light | Lumière de Marée | Luz de Marea | Flow, adaptability, currents of fate | "Water finds all paths" |
| J | Forge Light | Lumière de Forge | Luz de Forja | Creation, artifice, constructed beings | "Mind shapes matter" |

### 3.3 Species (15) — Final Names

| Order | Magenta | Crimson | Bronze |
|-------|---------|---------|--------|
| F (Shadow Light) | **Bloodmage** | **Revenant** | **Spectre** |
| G (Deep Light) | **Deep Gnome** | **Stone Troll** | **Forgeborn** |
| H (Crystal Light) | **Dryad** | **Crystalkin** | **Gemsoul** |
| I (Tide Light) | **Merfolk** | **Deepborn** | **Coralsmith** |
| J (Forge Light) | **Golem** | **Fleshcraft** | **Ironbound** |

### 3.4 Species Notes

- **Bloodmage** (was "Vampire" in early draft) — renamed to avoid collision with the existing Vampire *ability*. Life-force manipulators, hemomancers; theme preserved.
- **Revenant** (was "Shade") — undead warriors, vengeful spirits.
- **Spectre** (was "Wraith") — ethereal assassins, phase-walkers.
- **Stone Troll** (was "Troll") — living rock, near-invulnerable.
- **Forgeborn** (was "Kobold") — metalworkers' constructs, living ore.
- **Crystalkin** (was "Nymph") — gemstone entities, faceted beings.
- **Gemsoul** (was "Unicorn") — amber-hearted spirits, petrified essence.
- **Deepborn** (was "Triton") — abyssal dwellers, trench hunters.
- **Coralsmith** (was "Siren") — reef architects, mineral shapers.
- **Fleshcraft** (was "Automaton") — biological constructs, grown not built.
- **Ironbound** (was "Warforged") — war machines, armored juggernauts.
- **Deep Gnome, Dryad, Merfolk, Golem** — unchanged from initial draft; no naming conflicts.

---

## 4. Expansion 2 — "The Celestial Ascent"

**Aesthetic:** Sky realms, divine beings, cosmic forces. Bright metallics, clouds, starlight, divine radiance.

### 4.1 Colors (3)

| Color | Name | HSL | Theme |
|-------|------|-----|-------|
| Gold | Sunstone | (45°, 100%, 50%) | Divine power, radiance, glory |
| Sky Blue | Sapphire | (210°, 80%, 55%) | Sky, wind, storms |
| Forest Green | Emerald | (140°, 60%, 40%) | Life, growth, vitality |

### 4.2 Orders (5)

| Order | EN | FR | ES | Theme | Philosophy |
|-------|----|----|----|-------|------------|
| K | Storm Light | Lumière d'Orage | Luz de Tormenta | Thunder, wind, aerial combat | "The sky calls to those who dare" |
| L | Radiant Light | Lumière Radieuse | Luz Radiante | Holy power, divine justice | "Truth burns away shadow" |
| M | Wild Light | Lumière Sauvage | Luz Salvaje | Primal nature, untamed beasts | "Instinct guides the worthy" |
| N | Star Light | Lumière Stellaire | Luz Estelar | Cosmic forces, astral beings | "Written in the heavens" |
| O | Life Light | Lumière de Vie | Luz de Vida | Growth, healing, renewal | "All things bloom in time" |

### 4.3 Species (15) — Final Names

| Order | Gold | Sky Blue | Forest Green |
|-------|------|----------|---------------|
| K (Storm Light) | **Phoenix** | **Thunderbird** | **Sylph** |
| L (Radiant Light) | **Angel** | **Archon** | **Seraph** |
| M (Wild Light) | **Griffon** | **Manticore** | **Chimera** |
| N (Star Light) | **Celestial** | **Astral** | **Starseer** |
| O (Life Light) | **Treant** | **Satyr** | **Dríade** |

### 4.4 Species Notes

No naming or color conflicts identified for Expansion 2 — all 15 species names carried through unchanged from the original draft.

---

## 5. Complete System Overview (Base + Both Expansions)

### 5.1 Colors (9 total)

| Set | Colors |
|-----|--------|
| Base | Red, Indigo, Orange |
| Expansion 1 | Magenta, Crimson, Bronze |
| Expansion 2 | Gold, Sky Blue, Forest Green |

### 5.2 Orders (15 total, A–O)

| Set | Orders | Species (base game, unchanged) |
|-----|--------|--------------------------------|
| Base | A Dawn Light, B Verdant Light, C Ember Light, D Eternal Light, E Moonlight | Human/Elf/Dwarf, Hobbit/Faun/Centaur, Orc/Goblin/Minotaur, Dragon/Cyclops/Fairy, Aven/Koatl/Lycan |
| Expansion 1 | F Shadow Light, G Deep Light, H Crystal Light, I Tide Light, J Forge Light | — see §3.3 |
| Expansion 2 | K Storm Light, L Radiant Light, M Wild Light, N Star Light, O Life Light | — see §4.3 |

### 5.3 Species (45 total)

- Base: 15 (Human through Lycan) — unchanged
- Expansion 1: 15 (Bloodmage through Ironbound) — see §3.3
- Expansion 2: 15 (Phoenix through Dríade) — see §4.3

### 5.4 Card Pool

| Set | Champions | Utility | Total |
|-----|-----------|---------|-------|
| Base | 102 | 18 | 120 |
| Expansion 1 | 102 | 18 | 120 |
| Expansion 2 | 102 | 18 | 120 |
| **Combined** | **306** | **54** | **360** |

---

## 6. Combined Play & Combo Rules

- **Combo priority unchanged:** Species match > Order match > Color match.
- **Monochrome deck rule:** must stay within a single color, which — since each color belongs to exactly one set — means monochrome decks are inherently single-set (e.g., all-Bronze is Expansion 1 only).
- **Custom/Random decks:** may freely mix champions from Base + either or both expansions; Order range simply extends to A–O and Color range to all 9 for combo-matching purposes.
- **Draft formats:**
  - Solomon 7×7 / Draft 12×8: any single 120-card set.
  - Draft 1-2-3: any two 120-card sets combined (240 cards) — Base+Exp1, Base+Exp2, or Exp1+Exp2.
  - New "Full Pool Draft" variant: all 360 cards, 6 rounds of 10-card deals (60/player), discard to 40-card deck.

---

## 7. Naming/Design Decision Log (Chronological Rationale)

1. **Initial draft** proposed Teal and Silver as two of Expansion 1's three colors, and "Vampire" as one of its species.
2. **Conflict 1 — Teal:** already reserved for three UI/branding elements (logo text, card back, GUI table top) at 181°–192°. Reusing it for champions would break the base game's UI/gameplay color separation. → **Replaced with Crimson**, then further refined below.
3. **Conflict 2 — Silver:** too close to existing Card Text Gray (0°, 0%, 30%); a near-neutral gray/silver champion color risks being confused with UI text elements and is hard to distinguish from Gray at a glance. → **Replaced with Bronze**.
4. **Conflict 3 — Vampire (species):** collides with the existing Vampire *champion ability* name from `game-depth-additions ideas.md`. → **Renamed to Bloodmage**, preserving the blood/life-drain theme.
5. **Conflict 4 — Violet as champion color:** Initial revision proposed Violet (274°, 53%, 42%) for Expansion 1's third champion color. This is the *exact* HSL value already used for "Draw 3 Cards Purple," a utility card-type marker with only 6 cards in the base deck. Reusing it for 34 champion cards would break instant color-based card recognition (players must context-switch between "is this a champion or a utility card?"). → **Replaced with Magenta (310°, 70%, 45°)** — 36° hue separation from Purple, 35° from Red, thematically consistent with "twilight/dusk" for the Shadow Realms set.
6. **Expansion 2 colors adjusted for separation:** Gold shifted to a true yellow-gold (45°) rather than closer to Orange (26°); Sky Blue lightened relative to Indigo (210° vs. 244°) to read as "sky" rather than "deep blue"; Forest Green darkened/shifted to true green (140°) distinct from both Teal (181°-192°) and utility Green (162°).
7. **General rule established:** champion colors must maintain ≥15° hue separation from each other and ≥30° buffer from the two reserved utility hues (162° Green, 274° Purple); UI/Teal/Gray hues are excluded from the champion color pool entirely.

---

## 8. Implementation Notes for Claude Code Handoff

**Do not implement directly from this document.** This is a design reference; a scoped implementation handout (following existing `REFACTORING.md` methodology) should be produced separately when work begins, likely in its own `ideas/` subfolder.

When that handout is produced, it should account for:

- Extending `ChampionColor`, `ChampionOrder`, and `ChampionSpecies` enums (see prior draft sketches in chat — 9 colors, 15 orders A–O, 45 species) without touching `combo_bonus.c` logic itself.
- Adding a `CardPoolType` / config option (`--cardpool=base|base+exp1|base+exp2|exp1+exp2|all`) to `cmdline.c` / `player_config.c`, consistent with existing option-parsing conventions.
- Keeping utility card colors (Purple/Green) as pure display constants, not part of `ChampionColor`, so they can never be assigned to a champion by mistake.
- Verifying function/file line-count targets (~35 lines/function, ~400 lines/file) are respected when these enums and any new lookup tables are added — likely means new data lives in its own constants file rather than growing `game_constants.c` past soft limits.
- Species/color/order name string tables (EN/FR/ES) as sketched in chat, for any localization work already in progress elsewhere in the project.

---

*End of handout — reflects final state of design chat as of this document's creation.*
