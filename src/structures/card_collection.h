// card_collection.h
// Specialized fixed-size array collections for card game

#ifndef CARD_COLLECTION_H
#define CARD_COLLECTION_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Hand Collection (max 10 cards in practice, but allow margin)
// ============================================================================
typedef struct {
    uint8_t cards[12];  // Allow some margin beyond max of 10
    uint8_t size;
} Hand;

void Hand_init(Hand* hand);
void Hand_add(Hand* hand, uint8_t card);
bool Hand_remove(Hand* hand, uint8_t card);
void Hand_clear(Hand* hand);
uint8_t Hand_get(const Hand* hand, uint8_t index);
bool Hand_contains(const Hand* hand, uint8_t card);

// ============================================================================
// CombatZone Collection (max 3 cards)
// ============================================================================
typedef struct {
    uint8_t cards[3];
    uint8_t size;
} CombatZone;

void CombatZone_init(CombatZone* zone);
void CombatZone_add(CombatZone* zone, uint8_t card);
bool CombatZone_remove(CombatZone* zone, uint8_t card);
void CombatZone_clear(CombatZone* zone);
uint8_t CombatZone_get(const CombatZone* zone, uint8_t index);

// ============================================================================
// Discard Collection (max 40 cards)
// ============================================================================
typedef struct {
    uint8_t cards[40];
    uint8_t size;
} Discard;

void Discard_init(Discard* discard);
void Discard_add(Discard* discard, uint8_t card);
bool Discard_remove(Discard* discard, uint8_t card);
void Discard_clear(Discard* discard);
uint8_t Discard_get(const Discard* discard, uint8_t index);

#endif // CARD_COLLECTION_H
