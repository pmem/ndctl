/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2022 Intel Corporation. All rights reserved. */
#ifndef _NDCTL_LIST_H_
#define _NDCTL_LIST_H_

#include <ccan/list/list.h>

#define list_add_sorted(head, n, node, cmp)                                    \
	do {                                                                   \
		struct list_head *__head = (head);                             \
		typeof(n) __iter, __next;                                      \
		typeof(n) __new = (n);                                         \
                                                                               \
		if (list_empty(__head)) {                                      \
			list_add(__head, &__new->node);                        \
			break;                                                 \
		}                                                              \
                                                                               \
		list_for_each (__head, __iter, node) {                         \
			if (cmp(__new, __iter) < 0) {                          \
				list_add_before(__head, &__iter->node,         \
						&__new->node);                 \
				break;                                         \
			}                                                      \
			__next = list_next(__head, __iter, node);              \
			if (!__next) {                                         \
				list_add_after(__head, &__iter->node,          \
					       &__new->node);                  \
				break;                                         \
			}                                                      \
			if (cmp(__new, __next) < 0) {                          \
				list_add_before(__head, &__next->node,         \
						&__new->node);                 \
				break;                                         \
			}                                                      \
		}                                                              \
	} while (0)

#endif /* _NDCTL_LIST_H_ */
