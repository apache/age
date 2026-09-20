#ifndef AGTTYPE_TRAVERSAL_H
#define AGTTYPE_TRAVERSAL_H

#include "postgres.h"

#include "utils/graphid.h"
#include "utils/agtype.h"

/*
* AGI_INLINE_ITERS - number of agtype_iterator entities to store on the stack
*/
#define AGI_INLINE_ITERS 8

/*
* first chunk capacity
*/
#define AGI_STACK_INITIAL_CAPACITY 16

typedef struct AgtypeIteratorChunk
{
    agtype_iterator *array;

    int capacity;
    int used;

    struct AgtypeIteratorChunk *prev;
    struct AgtypeIteratorChunk *next;
} AgtypeIteratorChunk;

/* 
* AgtypeIteratorStack - chunked linked-list stack for DFS.
* The first chunk of size AGI_INLINE_ITERS is always stored on the stack.
*/
typedef struct AgtypeIteratorStack
{
    AgtypeIteratorChunk first;
    AgtypeIteratorChunk *top;
} AgtypeIteratorStack;

typedef struct agtype_traversal
{
    agtype_iterator inline_iters[AGI_INLINE_ITERS];

    agtype_iterator *it;

    AgtypeIteratorStack stack;
} agtype_traversal;


void init_agi_stack(AgtypeIteratorStack *stack,
                    agtype_iterator *inline_array,
                    int inline_capacity);
void free_agi_stack(AgtypeIteratorStack *stack);
agtype_iterator* agi_stack_reserve_next(AgtypeIteratorStack *s);
void agi_stack_pop(AgtypeIteratorStack *s);
agtype_iterator* agi_stack_peek(AgtypeIteratorStack *s);
bool agi_stack_is_empty(AgtypeIteratorStack *s);


void init_agtype_traversal(agtype_traversal* traversal);
agtype_iterator* free_and_get_parent(agtype_traversal* traversal);
agtype_iterator* prepare_next_iter(agtype_traversal* traversal);
void free_agtype_traversal(agtype_traversal* traversal);

#endif
