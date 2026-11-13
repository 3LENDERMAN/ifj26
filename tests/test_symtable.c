#include <stdio.h>
#include <assert.h>

#include "../src/symtable.h" 

int main() {
    printf("Initialize table\n");
    symtable_stack_t *stack = symtable_init();
    assert(stack != NULL);

    printf("Register builtin functions\n");
    symtable_register_builtins(stack);
    
    // test:find builtin function 'print'
    ht_item_t *print_fn = symtable_lookup_func(stack, "print");
    assert(print_fn != NULL);
    assert(print_fn->data.kind == SYMBOL_FUNC);
    printf("TEST on 'print()': function loaded\n");

    printf("Create global variable 'x'\n");
    symtable_insert_var(stack, "x", TYPE_INT, false);

    printf("Enter 'if' block (new scope) \n");
    symtable_enter_scope(stack); 
    
    printf("Create local variable 'x' - test of variable shadowing\n");
    symtable_insert_var(stack, "x", TYPE_STRING, false);
    
    // should get local 'x' string variable
    ht_item_t *local_x = symtable_lookup_var(stack, "x");
    assert(local_x != NULL);
    assert(local_x->data.type == TYPE_STRING);
    printf("TEST on variable shadowing passed\n");

    printf("Exit 'if' block (end scope)\n");
    symtable_exit_scope(stack); 

    // after exiting the block, 'x' should be of type int
    ht_item_t *global_x = symtable_lookup_var(stack, "x");
    assert(global_x != NULL);
    assert(global_x->data.type == TYPE_INT);
    printf("Old variable 'x' restored\n");

    symtable_free(stack);

    printf("All tests passed\n");
    return 0;
}
