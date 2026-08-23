# Keeper → Borealis: Naming Decision Summary

**Status:** Final — ready for implementation via find/replace
**Date:** 2026-07-27

---

## Decision

The benchmark AI agent previously called **Keeper** is renamed to **Borealis** (with localized forms below). Shared letter across all three languages: **B**.

| Language | Term | IPA | Notes |
|----------|------|-----|-------|
| English | **Borealis** | [ˌbɔːriˈælɪs] | Used as noun / noun adjunct |
| Français | **Boréal** | [bɔʁeal] | Adjective form preferred over "Boréalis" (more natural) |
| Español | **Boreal** | [boɾeˈal] | Invariant adjective form |

**Etymology note (optional, for docs):** From *Boreas*, Greek god of the north wind, and the *Aurora Borealis*. Evokes a fixed northern reference point — apt for a stable benchmark.

---

## Strength & Rating Terminology

Adjective forms in French and Spanish were chosen as most natural for native speakers.

| Language | Strength | Rating |
|----------|----------|--------|
| English | **Borealis strength** | **Borealis rating** |
| Français | **force boréale** | **cote boréale** |
| Español | **fuerza boreal** | **puntuación boreal** |

**Note on Spanish "rating":** *Puntuación* was chosen over *clasificación* (which implies ranking/standing position, not a numerical score) and over *calificación* (more academic/evaluative connotation). *Puntuación* best captures a calculated numerical score, consistent with the Bradley-Terry strength/rating model.

---

## Formula Notation

Universal symbol: **B**

```
S_B = 1.0   // Borealis / force boréale / fuerza boreal — strength
R_B = 50    // Borealis / cote boréale / puntuación boreal — rating
```

- No conflicts with existing formula variables (S = strength, R = rating; B was free)
- Avoided letters: **K** (original), **S** and **R** (already used for strength/rating themselves)

---

## Sample Usage

**English:**
> The Borealis agent serves as the fixed benchmark (Borealis strength = 1.0, Borealis rating = 50).

**Français:**
> L'agent Boréal sert de référence fixe (force boréale = 1,0, cote boréale = 50).

**Español:**
> El agente Boreal sirve como referencia fija (fuerza boreal = 1,0, puntuación boreal = 50).

---

## Code Constants Example

```c
// Borealis benchmark constants
// English: Borealis (strength = 1.0, rating = 50)
// Français: Boréal (force boréale = 1,0, cote boréale = 50)
// Español: Boreal (fuerza boreal = 1,0, puntuación boreal = 50)

#define BOREALIS_STRENGTH 1.0
#define BOREALIS_RATING 50.0

// Symbol in formulas: B
// S_B = Borealis strength
// R_B = Borealis rating
```

---

## Files Requiring "Keeper" → "Borealis" Replacement

### Rating system implementation
(Path corrected 2026-08-23 -- `ideas/14 rating system/` was renamed/renumbered to
`ideas/5 rating system/` before this checklist was ever acted on; check these files for
any surviving "Keeper" references when the rating system is actually implemented, since
`A3` is now shipped as "Borealis" throughout `src/`.)
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/oracle_rating_system.h`
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/oracle_rating_system.c`
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/oracle_rating_test.c`
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/oracle_rating_guide.md`
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/oracle_rating_readme.md`
- `ideas/5 rating system/v2 Bradley-Terry (BT) Rating System/rating_strength_analysis.html`

### Main documentation
- `README.md`
- `doc/oracle_design.md`
- `doc/oracle_roadmap.md`

### Find/replace variants to check for
- `Keeper` → `Borealis`
- `keeper` → `borealis`
- `KEEPER` → `BOREALIS`
- `keeper_id` → `borealis_id`
- `KEEPER_RATING` → `BOREALIS_RATING`
- `PLAYER_TYPE_AI_KEEPER` → `PLAYER_TYPE_AI_BOREALIS`
- `"Keeper"` (string literals, e.g. player name, CSV type labels)

---

## Rejected Alternatives (for reference)

| Name | Shared Letter | Reason Not Chosen |
|------|---------------|--------------------|
| Anchor / Ancre / Ancla | A | Conflicts with adaptive-rate `A(n)` notation |
| Baseline | B | Less evocative than Borealis |
| Pivot | P | Suggests movement rather than stability |
| Norm / Norme / Norma | N | Less memorable |
| Fulcrum / Fulcrum / Fulcro | F | French borrowing felt less natural to pronounce (led to "Foulcromme" workaround) |
| Beacon / Balise / Baliza | B | Implies active guidance more than passive reference |
| Origin / Origine / Origen | O | Implies "first" rather than fixed reference |
| Unity / Unité / Unidad | U | Fits strength=1.0 but abstract |
| Datum / Datum / Dato | D | Too technical/obscure |
| Terminus / Terminus / Término | T | Implies "end" rather than center |
| Lodestar / Étoile polaire / Estrella polar | E (FR/ES only) | English doesn't share the letter; three distinct word forms |
| Buoy / Bouée / Boya | B | Considered strong finalist; less distinctive than Borealis |
| Polaris / Polaris / Polaris | P | Considered top finalist (identical spelling in all 3 languages) but ultimately less preferred than Borealis |
