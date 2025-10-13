#ifndef __HDCLL_H
#define __HDCLL_H

// struct to host hand, discard, combat zone
struct LLNode {
    uint8_t data;            // Data stored in the node
    struct LLNode* next;   // Pointer to the next node
};

struct HDCLList {
    struct LLNode* head;         // Pointer to the first node
    uint8_t size;           // Tracks the number of nodes in the list
};

void HDCLL_initialize(struct HDCLList*);
void HDCLL_insertNodeAtBeginning(struct HDCLList*, uint8_t );
uint8_t HDCLL_removeNodeFromBeginning(struct HDCLList*);
uint8_t HDCLL_getNodeValueByIndex(struct HDCLList*, uint8_t);
uint8_t* HDCLL_toArray(struct HDCLList*);
struct LLNode* HDCLL_removeNodeByIndex(struct HDCLList*, uint8_t );
struct LLNode* HDCLL_removeNodeByValue(struct HDCLList*, uint8_t);
void HDCLL_emptyOut(struct HDCLList*);
void HDCLL_printLinkedList(struct HDCLList*);

#endif /* #ifndef __HDCLL_H */
