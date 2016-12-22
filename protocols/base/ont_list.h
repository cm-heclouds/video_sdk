#ifndef _ONT_LIST_H_
#define _ONT_LIST_H_

#include "ont/platform.h"

# ifdef __cplusplus
extern "C" {
# endif

typedef struct ont_list_node_t ont_list_node_t;
typedef struct ont_list_t ont_list_t;

/**
* @brief Use to process element data.
*/
typedef void(*process_data)(void *data, void *context);

/**
 * [create a list]
 * @return [description]
 */
ont_list_t* ont_list_create();

/**
 * [destory the list]
 * @param list [description]
 */
void ont_list_destroy(ont_list_t* list);

/**
 * [clear all nodes of the list ]
 * @param list [description]
 */
void ont_list_clear(ont_list_t* list);

/**
 * [get node number of the list]
 * @param  list [description]
 * @return      [description]
 */
size_t ont_list_size(ont_list_t* list);

/**
 * [ont_list_insert description]
 * @param  list [description]
 * @param  data [description]
 * @return      [description]
 */
ont_list_node_t* ont_list_insert(ont_list_t* list, void* data);

/**
 * [ont_list_insert_at description]
 * @param  list [description]
 * @param  node [description]
 * @param  data [description]
 * @return      [description]
 */
ont_list_node_t* ont_list_insert_at(ont_list_t* list, ont_list_node_t* at,void* data);

/**
* [ont_list_pop_front description]
* @param  list [description]
* @return      [description]
*/
int ont_list_pop_front(ont_list_t* list, void** data);
/**
 * [ont_list_remove description]
 * @param  list [description]
 * @param  node [description]
 * @param  data [description]
 * @return      [description]
 */
int ont_list_remove(ont_list_t* list, ont_list_node_t* node,void ** data);

/**
 * [ont_list_data description]
 * @param node [description]
 */
void* ont_list_data(ont_list_node_t* node);

/**
 * [ont_list_next description]
 * @param  node [description]
 * @return      [description]
 */
ont_list_node_t* ont_list_next(ont_list_node_t* node);


/**
 * [ont_list_foreach description]
 * @param  list    [description]
 * @param  process [description]
 * @param  context [description]
 * @return         [description]
 */
int ont_list_foreach(ont_list_t *list, process_data process, void *context);


/**
* @brief Use to process element data.
*/
typedef int (*compare_data)(void *data1, void *data2);
/**
 * [ont_list_find description]
 * @param  list [description]
 * @param  data2 [description]
 * @return      [description]
 */

ont_list_node_t* ont_list_find(ont_list_t *list, void * data2, compare_data compare);

# ifdef __cplusplus
}      /* extern "C" */
# endif

#endif