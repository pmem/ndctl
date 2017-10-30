/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#ifndef _NDCTL_LIST_H_
#define _NDCTL_LIST_H_

#include <ccan/list/list.h>

/**
 * list_add_after - add an entry after the given node in the linked list.
 * @h: the list_head to add the node to
 * @l: the list_node after which to add to
 * @n: the list_node to add to the list.
 *
 * The list_node does not need to be initialized; it will be overwritten.
 * Example:
 *	struct child *child = malloc(sizeof(*child));
 *
 *	child->name = "geoffrey";
 *	list_add_after(&parent->children, &child1->list, &child->list);
 *	parent->num_children++;
 */
#define list_add_after(h, l, n) list_add_after_(h, l, n, LIST_LOC)
static inline void list_add_after_(struct list_head *h,
				   struct list_node *l,
				   struct list_node *n,
				   const char *abortstr)
{
	if (l->next == &h->n) {
		/* l is the last element, this becomes a list_add_tail */
		list_add_tail(h, n);
		return;
	}
	n->next = l->next;
	n->prev = l;
	l->next->prev = n;
	l->next = n;
	(void)list_debug(h, abortstr);
}

#endif /* _NDCTL_LIST_H_ */
