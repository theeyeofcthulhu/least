#ifndef STACK_H_
#define STACK_H_

typedef struct{
    int cap;
    int counter;
    int* stack_arr;
}int_stack;

int_stack* int_stack_create(int cap);
void int_stack_push(int_stack* stack, int num);
int int_stack_pop(int_stack* stack);
int int_stack_pull(int_stack* stack);
int int_stack_top(int_stack* stack);
int int_stack_bottom(int_stack* stack);
void int_stack_free(int_stack* stack);

typedef struct{
    int cap;
    int counter;
    char** stack_arr;
}str_stack;

str_stack* str_stack_create(int cap);
void str_stack_push(str_stack* stack, char* str);
char* str_stack_pop(str_stack* stack);
char* str_stack_pull(str_stack* stack);
char* str_stack_top(str_stack* stack);
char* str_stack_bottom(str_stack* stack);
void str_stack_free(str_stack* stack);
char* str_stack_get(str_stack* stack, int n);

#endif // STACK_H_
