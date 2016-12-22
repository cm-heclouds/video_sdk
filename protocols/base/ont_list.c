#include "ont_list.h"
#include "ont_base_common.h"
#include "ont/error.h"

struct ont_list_node_t
{
	void                      *data;
	struct ont_list_node_t    *next;
};

struct ont_list_t {
	ont_list_node_t    *head;
	ont_list_node_t    *tail;
	size_t             size;
};

static ont_list_node_t *create_node(void *data)
{
	ont_list_node_t *node = NULL;
	ONT_RETURN_IF_NIL(data, NULL);
	node = (ont_list_node_t *)ont_platform_malloc(sizeof(ont_list_node_t));
	ONT_RETURN_IF_NIL(node, NULL);
	node->data = data;
	node->next = NULL;
	return node;
}


ont_list_t* ont_list_create()
{
	ont_list_t * list = (ont_list_t*)ont_platform_malloc(sizeof(ont_list_t));
	ONT_RETURN_IF_NIL(list, NULL)

	list->head = list->tail = NULL;
	list->size = 0;

	return  list;
}

void ont_list_destroy(ont_list_t* list)
{
	if (!list)
	{
		return;
	}
	ont_list_clear(list);
	ont_platform_free(list);
}

void ont_list_clear(ont_list_t* list)
{
	ont_list_node_t * it = NULL;
	ont_list_node_t *next = NULL;
	if (!list)
	{
		return;
	}

	it = list->head;
	while (it)
	{
		next = it->next;
		ont_platform_free(it);
		it = next;
	}
	list->head = list->tail = NULL;
	list->size = 0;
}

size_t ont_list_size(ont_list_t* list)
{
	ONT_RETURN_IF_NIL(list, ONT_ERR_BADPARAM);
	return list->size;
}

ont_list_node_t* ont_list_insert(ont_list_t* list, void* data)
{
	ont_list_node_t * node = NULL;
	ONT_RETURN_IF_NIL2(list,data, NULL);
	node = create_node(data);
	ONT_RETURN_IF_NIL(node, NULL);

	/* list is empty */
	if (list->head == NULL)
	{
		list->head = node ;
		list->tail = node ;
	}
	/* list is not empty */
	else
	{
		list->tail->next = node ;
		list->tail = node;
	}
	++list->size;
	return node;
}

ont_list_node_t* ont_list_insert_at(ont_list_t* list, ont_list_node_t* at, void* data)
{
	ont_list_node_t * new_node = NULL;
	ont_list_node_t * old_neighbor = NULL;
	ont_list_node_t * it = NULL;
	ONT_RETURN_IF_NIL3(list, at, data, NULL);

	/* if list is empty */
	ONT_RETURN_IF_NIL(list->head, NULL);

	new_node = create_node(data);
	ONT_RETURN_IF_NIL(new_node, NULL);

	/* if node is head node */
	if (list->head == at)
	{
		++list->size;
		new_node->next = list->head->next;
		list->head->next = new_node;
		return ONT_ERR_OK;
	}

	it = list->head;
	do
	{
		if (it->next == at)
		{
			++list->size;
			/* if node is tail node */
			if (at == list->tail)
			{
				list->tail->next = new_node;
				list->tail = new_node;
				return ONT_ERR_OK;
			}

			/* node is in middle of list */
			new_node->next = at->next;
			at->next = new_node;
			return ONT_ERR_OK;
		}
		it = it->next;
	} while (it->next);

	old_neighbor = at->next;
	at->next = new_node;
	new_node->next = old_neighbor;

	return new_node;
}

int ont_list_remove(ont_list_t* list, ont_list_node_t* node,void ** data)
{
	ont_list_node_t * it = NULL; 
	ONT_RETURN_IF_NIL2(list, node, ONT_ERR_BADPARAM);

	/* if node is head node */
	if (list->head == node)
	{ 
		--list->size;
		if (data)
		{
			*data = node->data;
		}
		list->head = list->head->next;
		if (list->tail == node)
		{
			list->tail = list->tail->next;
		}
		ont_platform_free(node);
		node = NULL;
		return ONT_ERR_OK;	
	}

	it = list->head;
	do
	{
		if (it->next == node)
		{
			--list->size;
			if (data)
			{
				*data = node->data;
			}
			
			/* if node is tail node */
			if (node == list->tail)
			{
				it->next = NULL;
				list->tail = it; 

				ont_platform_free(node);
				return ONT_ERR_OK;
			}

			it->next = it->next->next;
			ont_platform_free(node);
			return ONT_ERR_OK;
		}
		it = it->next;
	} while (it->next);

	return ONT_ERR_OK;
}

int ont_list_pop_front(ont_list_t* list, void** data)
{
	ONT_RETURN_IF_NIL2(list, data,ONT_ERR_BADPARAM);

	return ont_list_remove(list, list->head, data);
}

int ont_list_foreach(ont_list_t *list, process_data process, void *context)
{
	ont_list_node_t * it = NULL;
	ont_list_node_t *next = NULL;
	ONT_RETURN_IF_NIL2(list, process,ONT_ERR_BADPARAM);
	
	it = list->head;
	while (it)
	{
		next = it->next;
		process(it->data, context);
		it = next;
	}
	return ONT_ERR_OK;
}

void* ont_list_data(ont_list_node_t* node)
{
	ONT_RETURN_IF_NIL(node, NULL)
	return node->data;
}

ont_list_node_t* ont_list_next(ont_list_node_t* node)
{
	ONT_RETURN_IF_NIL(node, NULL)
	return node->next;
}

ont_list_node_t* ont_list_find(ont_list_t *list, void * data2, compare_data compare)
{
	ont_list_node_t * it = NULL;
	ont_list_node_t *next = NULL;
	ONT_RETURN_IF_NIL2(list, data2, NULL);

	it = list->head;
	while (it)
	{
		next = it->next;
		if (0 == compare(it->data, data2))
		{
			return it;
		}
		it = next;
	}
	return NULL;
}