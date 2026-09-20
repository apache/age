
#include <utils/agtype_traversal.h>

#define AGI_STACK_INITIAL_CAPACITY 16

void init_agi_stack(AgtypeIteratorStack *stack,
                    agtype_iterator *inline_array,
                    int inline_capacity)
{
    stack->first.array = inline_array;
    stack->first.capacity = inline_capacity;
    stack->first.used = 0;
    stack->first.prev = NULL;
    stack->first.next = NULL;

    stack->top = &stack->first;
}


void free_agi_stack(AgtypeIteratorStack *stack)
{
    AgtypeIteratorChunk *chunk = stack->first.next;

    while (chunk != NULL)
    {
        AgtypeIteratorChunk *next = chunk->next;

        pfree(chunk->array);
        pfree(chunk);

        chunk = next;
    }

    stack->first.used = 0;
    stack->first.next = NULL;
    stack->top = &stack->first;
}

agtype_iterator * agi_stack_reserve_next(AgtypeIteratorStack *stack)
{
    AgtypeIteratorChunk *chunk = stack->top;

    if (chunk->used == chunk->capacity)
    {
        if (chunk->next == NULL)
        {
            AgtypeIteratorChunk *next = palloc(sizeof(*next));

            next->capacity = Max(chunk->capacity * 2,
                                 AGI_STACK_INITIAL_CAPACITY);
            next->used = 0;
            next->array =
                palloc(sizeof(agtype_iterator) * next->capacity);

            next->prev = chunk;
            next->next = NULL;

            chunk->next = next;
        }

        chunk = chunk->next;
        chunk->used = 0;

        stack->top = chunk;
    }

    return &chunk->array[chunk->used++];
}

void agi_stack_pop(AgtypeIteratorStack *stack)
{
    AgtypeIteratorChunk *chunk = stack->top;

    Assert(chunk->used > 0);

    chunk->used--;

    if (chunk->used == 0 && chunk->prev != NULL)
        stack->top = chunk->prev;
}

agtype_iterator * agi_stack_peek(AgtypeIteratorStack *stack)
{
    AgtypeIteratorChunk *chunk = stack->top;

    Assert(chunk != NULL);
    Assert(chunk->used > 0);

    return &chunk->array[chunk->used - 1];
}

bool agi_stack_is_empty(AgtypeIteratorStack *stack)
{
    return stack->top == &stack->first &&
        stack->first.used == 0;
}

void init_agtype_traversal(agtype_traversal *traversal)
{
    init_agi_stack(&traversal->stack,
                   traversal->inline_iters,
                   AGI_INLINE_ITERS);

    traversal->it = agi_stack_reserve_next(&traversal->stack);
}

agtype_iterator * free_and_get_parent(agtype_traversal *traversal)
{
    agi_stack_pop(&traversal->stack);

    if (agi_stack_is_empty(&traversal->stack))
        return NULL;

    return agi_stack_peek(&traversal->stack);
}

agtype_iterator* prepare_next_iter(agtype_traversal* traversal)
{
    traversal->it = agi_stack_reserve_next(&(traversal->stack));
    return traversal->it;
}

void free_agtype_traversal(agtype_traversal *traversal)
{
    if (traversal->stack.top == NULL)
        return;

    free_agi_stack(&traversal->stack);
    traversal->stack.top = NULL;
    traversal->it = NULL;
}
