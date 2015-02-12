/*| headers |*/
#include <stdint.h>

/*| object_like_macros |*/

/*| type_definitions |*/

/*| structure_definitions |*/

/*| extern_definitions |*/

/*| function_definitions |*/

/*| state |*/
/*
 * PowerPC EABI stack is 8-byte aligned.
 * See ppce500-context-switch.c
 */
{{#tasks}}
static uint32_t stack_{{idx}}[{{stack_size}}] __attribute__((aligned(8)));
{{/tasks}}

/*| function_like_macros |*/
#define stack_init()

/*| functions |*/

/*| public_functions |*/
