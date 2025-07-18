#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "hdcll.h"

void HDCLL_initialize(struct HDCLList* list) {
//    list = (LinkedList*)malloc(sizeof(LinkedList));
//    if (list == NULL) {
//        // Handle memory allocation error
//        return NULL;
//    }
    list->head = NULL;
    list->size = 0;
//    return list;
}

void HDCLL_insertNodeAtBeginning(struct HDCLList* list, uint8_t data) {
    struct LLNode* newNode = (struct LLNode*)malloc(sizeof(struct LLNode));
    if (newNode == NULL) {
        // Handle memory allocation error
        return;
    }
    newNode->data = data;
    newNode->next = list->head;
    list->head = newNode;
    list->size++; // Increment size
}

 /*
   * To insert a node in a sorted linked list, first create a new node with the desired value. 
   * Then, traverse the list to find the correct position where the new node should be inserted, 
   * ensuring the list remains sorted. If the list is empty or the new node's value is smaller 
   * than the head, insert it at the beginning; otherwise, find the appropriate node and insert 
   * it after that node.
   */
// not currently used
/*
void HDCLL_insertNodeSorted(struct HDCLList* list, int data)
{
 
   
}
*/


uint8_t HDCLL_removeNodeFromBeginning(struct HDCLList* list) {
    if (list->head == NULL) {
        // List is empty
        return -1; // Or handle error appropriately
    }
    struct LLNode* temp = list->head;
    uint8_t data = temp->data;
    list->head = list->head->next;
    free(temp);
    list->size--; // Decrement size
    return data;
}


/*
int HDCLL_getSize(struct HDCLList* list) {
    return list->size;
}
*/

// Function to remove a node by its data value
struct LLNode* HDCLL_removeNodeByValue(struct HDCLList* list, uint8_t value_to_remove) {
    if (list->head == NULL) {
        return NULL; // List is empty
    }

    // If the head node is the one to be removed
    if (list->head->data == value_to_remove) {
        struct LLNode* temp = list->head;
        list->head = list->head->next;
        free(temp);
        list->size--;
        return list->head;
    }

    struct LLNode* current = list->head;
    struct LLNode* prev = NULL;

    // Traverse the list to find the node and its predecessor
    while (current != NULL && current->data != value_to_remove) {
        prev = current;
        current = current->next;
    }

    // If the value was not found
    if (current == NULL) 
    {
        printf("Node with value %d not found.\n", value_to_remove);    
    }
    else
    {
      // Adjust links and free memory
      prev->next = current->next;
      free(current);
      list->size--;
    }
    return list->head;
} // HDCLL_removeNodeByValue

// to delete a node by index
struct LLNode* HDCLL_removeNodeByIndex(struct HDCLList* list, uint8_t index) {
    if (list->head == NULL) {
        printf("List is empty. Cannot delete.\n");
        return NULL;
    }

    struct LLNode* current = list->head;
    struct LLNode* previous = NULL;
    int count = 0;

    // Handle deletion of the head node (index 0)
    if (index == 0) {
        list->head = list->head->next;
        free(current);
        list->size--;
        return list->head;
    }

    // Traverse to the node before the target
    while (current != NULL && count < index) {
        previous = current;
        current = current->next;
        count++;
    }

    // Perform deletion if the node is found
    if (current != NULL) {
        previous->next = current->next;        
        free(current);
        list->size--;
    } else {
        printf("Index out of bounds. Node not found.\n");
    }

    return list->head;
} // HDCLL_removeNodeByIndex

// This will return the node value for a given index in the Linked List. It will not remove the node.
uint8_t HDCLL_getNodeValueByIndex(struct HDCLList* card_list, uint8_t card_index)
{
  struct LLNode* current = card_list->head;
  uint8_t i = 0;
  while (i < card_index)
  {
    current = current->next;
    i++;
  }
  
  return current->data;
} // HDCLL_getNodeValueByIndex

// Helper function to print the list (for testing).
void HDCLL_printLinkedList(struct HDCLList* list) {
    struct LLNode* current = list->head;
    printf("(");
    while (current != NULL) {
        printf("%u, ", current->data);
        current = current->next;
    }
    printf(")");
}


/*
char* HDCLL_toString(struct LLNode* head)
{
  struct LLNode* current = head;
  while (current != NULL) {
    printf("%u, ", current->data);
    current = current->next;
  }
  return str;
}
*/

// creates a plain array with the values from the HDC Linked List. Don't forget to free() the memory once the array is no longer needed
uint8_t* HDCLL_toArray(struct HDCLList* list) {
    struct LLNode* current = list->head;
    uint8_t* A = (uint8_t*)malloc(list->size*sizeof(uint8_t));
    uint8_t i = 0;
    while (current != NULL) {
        A[i++] = current->data;
        current = current->next;
    }
    return A;
}

// free all Linked List heap memory
void HDCLL_emptyOut(struct HDCLList* list) {
  if (list->head == NULL) {
    return;  // List is empty already
  }
  struct LLNode* current;
  //printf("Debug: In %s, file %s, line %d\n", __func__, __FILE__, __LINE__);
  while (list->head != NULL) {
    current = list->head;
    list->head = list->head->next;
    free(current);
    list->size--;
    //printf("Debug: In %s, file %s, line %d, list size %d\n", __func__, __FILE__, __LINE__, list->size); 
  }
} // HDCLL_emptyOut
