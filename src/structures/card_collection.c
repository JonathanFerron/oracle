// card_collection.c
// Implementation of specialized card collections

#include "card_collection.h"

// ============================================================================
// Hand Implementation
// ============================================================================

void Hand_init(Hand* hand) {
    hand->size = 0;
}

void Hand_add(Hand* hand, uint8_t card) {
    if (hand->size < 12) {
        hand->cards[hand->size++] = card;
    }
}

bool Hand_remove(Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            // Shift remaining cards left
            for (uint8_t j = i; j < hand->size - 1; j++) {
                hand->cards[j] = hand->cards[j + 1];
            }
            hand->size--;
            return true;
        }
    }
    return false;
}

bool Hand_remove_unordered(Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            // Swap with last element instead of shifting!
            hand->cards[i] = hand->cards[hand->size - 1];
            hand->size--;
            return true;
        }
    }
    return false;
}

void Hand_clear(Hand* hand) {
    hand->size = 0;
}

uint8_t Hand_get(const Hand* hand, uint8_t index) {
    if (index < hand->size) {
        return hand->cards[index];
    }
    return 0;
}

bool Hand_contains(const Hand* hand, uint8_t card) {
    for (uint8_t i = 0; i < hand->size; i++) {
        if (hand->cards[i] == card) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// CombatZone Implementation
// ============================================================================

void CombatZone_init(CombatZone* zone) {
    zone->size = 0;
}

void CombatZone_add(CombatZone* zone, uint8_t card) {
    if (zone->size < 3) {
        zone->cards[zone->size++] = card;
    }
}

bool CombatZone_remove(CombatZone* zone, uint8_t card) {
    for (uint8_t i = 0; i < zone->size; i++) {
        if (zone->cards[i] == card) {
            for (uint8_t j = i; j < zone->size - 1; j++) {
                zone->cards[j] = zone->cards[j + 1];
            }
            zone->size--;
            return true;
        }
    }
    return false;
}

void CombatZone_clear(CombatZone* zone) {
    zone->size = 0;
}

uint8_t CombatZone_get(const CombatZone* zone, uint8_t index) {
    if (index < zone->size) {
        return zone->cards[index];
    }
    return 0;
}

// ============================================================================
// Discard Implementation
// ============================================================================

void Discard_init(Discard* discard) {
    discard->size = 0;
}

void Discard_add(Discard* discard, uint8_t card) {
    if (discard->size < 40) {
        discard->cards[discard->size++] = card;
    }
}

bool Discard_remove(Discard* discard, uint8_t card) {
    for (uint8_t i = 0; i < discard->size; i++) {
        if (discard->cards[i] == card) {
            for (uint8_t j = i; j < discard->size - 1; j++) {
                discard->cards[j] = discard->cards[j + 1];
            }
            discard->size--;
            return true;
        }
    }
    return false;
}

bool Discard_remove_unordered(Discard* discard, uint8_t card) {
    for (uint8_t i = 0; i < discard->size; i++) {
        if (discard->cards[i] == card) {
            // Swap with last element instead of shifting!
            discard->cards[i] = discard->cards[discard->size - 1];
            discard->size--;
            return true;
        }
    }
    return false;
}

void Discard_clear(Discard* discard) {
    discard->size = 0;
}

uint8_t Discard_get(const Discard* discard, uint8_t index) {
    if (index < discard->size) {
        return discard->cards[index];
    }
    return 0;
}
