/*
NAME: Chaoran Lin
EMAIL: linmc@ucla.edu
ID: 004674598
*/

#include "SortedList.h"
#include <sched.h>
#include <string.h>

#include <stdio.h>

int opt_yield;

/**
 * SortedList_insert ... insert an element into a sorted list
 *
 *	The specified element will be inserted in to
 *	the specified list, which will be kept sorted
 *	in ascending order based on associated keys
 *
 * @param SortedList_t *list ... header for the list
 * @param SortedListElement_t *element ... element to be added to the list
 */
void SortedList_insert(SortedList_t *list, SortedListElement_t *element) 
{
  SortedList_t * curr = list;
  if (curr->next == NULL) {
    if (opt_yield & INSERT_YIELD)
      sched_yield();
    curr->next = element;
    element->prev = curr;
    element->next = NULL;
    return;
  }
 
  // Traverse list
  while (curr->next != NULL && strcmp(curr->next->key, element->key) < 0) {
    if (opt_yield & INSERT_YIELD)
      sched_yield();
    curr = curr->next;
  }
    
  // Swap pointers and insert
  element->next = curr->next;
  element->prev = curr;
  curr->next = element;
  if (element->next != NULL)
    element->next->prev = element;
}

/**
 * SortedList_delete ... remove an element from a sorted list
 *
 *	The specified element will be removed from whatever
 *	list it is currently in.
 *
 *	Before doing the deletion, we check to make sure that
 *	next->prev and prev->next both point to this node
 *
 * @param SortedListElement_t *element ... element to be removed
 *
 * @return 0: element deleted successfully, 1: corrtuped prev/next pointers
  if (opt_yield & DELETE_YIELD)
    sched_yield();

 *
 */
int SortedList_delete( SortedListElement_t *element)
{
  if (opt_yield & DELETE_YIELD)
    sched_yield();

  if (element == NULL)
    return 1;
  
  // Check neighboring nodes

  if (element->next != NULL && element->next->prev != element)
    return 1;
  if (element->prev != NULL && element->prev->next != element)
    return 1;

  // If all ok then delete
  if (element->next != NULL)
    element->next->prev = element->prev;
  if (element->prev != NULL)
    element->prev->next = element->next;

  return 0;
}

/**
 * SortedList_lookup ... search sorted list for a key
 *
 *	The specified list will be searched for an
 *	element with the specified key.
 *
 * @param SortedList_t *list ... header for the list
 * @param const char * key ... the desired key
 *
 * @return pointer to matching element, or NULL if none is found
 */
SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
  if (list == NULL || list->next == NULL)
    return NULL;

  // Traverse list and return key immediately if found
  SortedList_t * curr = list->next;
  while (curr != NULL) {
    if (opt_yield & LOOKUP_YIELD)
      sched_yield();
    if (strcmp(curr->key, key) == 0)
      return curr;
    curr = curr->next;
  }
  
  // Else key does not exist in list, return NULL
  return NULL;
}

/**
 * SortedList_length ... count elements in a sorted list
 *	While enumeratign list, it checks all prev/next pointers
 *
 * @param SortedList_t *list ... header for the list
 *
 * @return int number of elements in list (excluding head)
 *	   -1 if the list is corrupted
 */
int SortedList_length(SortedList_t *list)
{
  if (list == NULL || list->next == NULL)
    return 0;

  SortedList_t * curr = list->next;
  int counter = 0;

  while (curr != NULL) {
    if (opt_yield & LOOKUP_YIELD)
      sched_yield();
    if (curr->prev != NULL && curr->prev->next != curr)
      return -1;
    if (curr->next != NULL && curr->next->prev != curr)
      return -1;
    curr = curr->next;
    counter++;
  }
  return counter;
}
