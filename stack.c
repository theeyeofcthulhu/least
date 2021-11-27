#include "stack.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

int_stack* int_stack_create(int cap){
    int_stack* result = malloc(sizeof(int_stack));
    result->cap = cap;
    result->counter = 0;
    result->stack_arr = malloc(cap * sizeof(int));
    memset(result->stack_arr, 0, cap * sizeof(int));

    return result;
}

void int_stack_push(int_stack* stack, int num){
    compiler_error_on_true(stack->counter >= stack->cap, "Internal", 0, "Compiler internal stackoverflow\n");
    stack->stack_arr[stack->counter++] = num;
}

int int_stack_pop(int_stack* stack){
    compiler_error_on_true(stack->counter == 0, "Internal", 0, "Compiler internal access to empty integer stack\n");
    return stack->stack_arr[--stack->counter];
}

int int_stack_pull(int_stack* stack){
    compiler_error_on_true(stack->counter == 0, "Internal", 0, "Compiler internal access to empty integer stack\n");
    int out = stack->stack_arr[0];
    for (int i = 0; i < stack->counter - 1; i++) {
        stack->stack_arr[i] = stack->stack_arr[i + 1];
    }
    stack->counter--;

    return out;
}

int int_stack_top(int_stack* stack){
    compiler_error_on_true(stack->counter == 0, "Internal", 0, "Compiler internal access to empty integer stack\n");
    return stack->stack_arr[stack->counter - 1];
}

int int_stack_bottom(int_stack* stack){
    compiler_error_on_true(stack->counter == 0, "Internal", 0, "Compiler internal access to empty integer stack\n");
    return stack->stack_arr[0];
}

void int_stack_free(int_stack* stack){
    compiler_error_on_false(stack->stack_arr || stack, "Internal", 0, "Compiler internal try to free empty stack\n");
    free(stack->stack_arr);
    free(stack);
}

str_stack* str_stack_create(int cap){
    str_stack* result = malloc(sizeof(str_stack));
    result->cap = cap;
    result->counter = 0;
    result->stack_arr = malloc(cap * sizeof(char*));
    memset(result->stack_arr, 0, cap * sizeof(char*));

    return result;
}

void str_stack_push(str_stack* stack, char* str){
    compiler_error_on_true(stack->counter >= stack->cap, "Internal", 0, "Compiler internal stackoverflow\n");
    stack->stack_arr[stack->counter++] = str;
}

char* str_stack_pop(str_stack* stack){
    if(stack->counter == 0)
        return NULL;
    return stack->stack_arr[--stack->counter];
}

char* str_stack_pull(str_stack* stack){
    if(stack->counter == 0)
        return NULL;
    char* out = stack->stack_arr[0];
    for (int i = 0; i < stack->counter - 1; i++) {
        stack->stack_arr[i] = stack->stack_arr[i + 1];
    }
    stack->counter--;

    return out;
}

char* str_stack_top(str_stack* stack){
    if(stack->counter == 0)
        return NULL;
    return stack->stack_arr[stack->counter - 1];
}

char* str_stack_bottom(str_stack* stack){
    if(stack->counter == 0)
        return NULL;
    return stack->stack_arr[0];
}

void str_stack_free(str_stack* stack){
    compiler_error_on_false(stack->stack_arr || stack, "Internal", 0, "Compiler internal try to free empty stack\n");
    free(stack->stack_arr);
    free(stack);
}
