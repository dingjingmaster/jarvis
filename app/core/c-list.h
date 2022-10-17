//
// Created by dingjing on 8/11/22.
//

#ifndef JARVIS_C_LIST_H
#define JARVIS_C_LIST_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdbool.h>

struct list_head
{
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) \
    { &(name), &(name) }

#define LIST_HEAD(name)     \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * @brief 初始化一个循环链表
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * @brief 在已知的两个节点之间插入新节点
 *
 * @param node 表示要插入的节点
 *
 */
static inline void __list_add(struct list_head *node, struct list_head *prev, struct list_head *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

/**
 * @brief 链表中头插新节点
 *
 * @param head 链表头
 * @param node 新节点
 */
static inline void list_add(struct list_head *node, struct list_head *head)
{
    __list_add(node, head, head->next);
}

/**
 * @brief 链表尾插新节点
 * @param head 要插入的链表
 * @param node 新节点
 */
static inline void list_add_tail(struct list_head *node, struct list_head *head)
{
    __list_add(node, head->prev, head);
}

/**
 * @brief 删除链表两个节点之间的节点
 *
 * @param prev 要删除节点的前一个节点
 * @parm next 要删除节点的后一个节点
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * @brief 从链表中删除指定节点
 *
 * @param entry 要删除的节点
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

/**
 * @brief 从原来节点处删除 list 节点，并把 list 节点头插到 head 链表
 *
 * @param list 要删除的节点
 * @param head 链表头
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/**
 * @brief 从原来节点处删除 list 节点，并把 list 节点尾插到 head 链表
 *
 * @param list 要删除的节点
 * @param head 要添加到的链表
 */
static inline void list_move_tail(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/**
 * @brief 检测链表是否为空
 *
 * @param 链表头节点
 *
 * @return 返回0表示为空，否则返回非0
 */
static inline bool list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * @brief 将 list 链表头插到 head链表处(两个链表合并)，合并完成后 list 这个节点无用了
 *
 * @param list 链表
 * @param head 链表
 */
static inline void __list_splice(struct list_head *list, struct list_head *head)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;
    struct list_head *at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}

/**
 * @brief
 *  1. 如果 list 和 head 在一个链表内，则将 list 节点与 head节点的下一个(head->next)节点互换位置
 *  2. 如果 list 和 head 属于两个不同的链表，则相当于把 list 链表追加到 head 列表处
 *
 * @param list 节点/另一个列表
 * @param head 头节点
 *
 * @note 如果list和head是两个链表，执行完此操作后 list 节点不要再用了
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head);
    }
}

/**
 * @brief 将list列表加入到head列表，并重新初始化list头节点
 *
 * @param list 链表
 * @param head 链表
 */
static inline void list_splice_init(struct list_head *list, struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head);
        INIT_LIST_HEAD(list);
    }
}

/**
 * @brief 节点 ptr 中获取 member 元素
 *
 * @param ptr 链表节点
 * @param type 数据类型
 * @param member 结构体中的成员
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * @brief 遍历链表(向后遍历)
 *
 * @param pos 位置
 * @param head 头节点
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * @brief 遍历链表(向前遍历)
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		 pos = n, n = pos->next)

/**
 * @brief 遍历链表(head)中的每一个元素(member)
 *  list_for_each_entry	- iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member) \
	for (pos = list_entry((head)->next, typeof (*pos), member); \
		 &pos->member != (head); \
		 pos = list_entry(pos->member.next, typeof (*pos), member))

/**
 * @brief 单链表 节点
 * Single-linked list. Added by Xie Han <xiehan@sogou-inc.com>.
 */
struct slist_node
{
    struct slist_node *next;
};

/**
 * @brief 单链表头
 */
struct slist_head
{
    struct slist_node first, *last;
};

#define SLIST_HEAD_INIT(name)	{ { (struct slist_node *)0 }, &(name).first }

#define SLIST_HEAD(name) \
	struct slist_head name = SLIST_HEAD_INIT(name)

static inline void INIT_SLIST_HEAD(struct slist_head *list)
{
    list->first.next = (struct slist_node *)0;
    list->last = &list->first;
}

static inline void slist_add_head(struct slist_node *node, struct slist_head *list)
{
    node->next = list->first.next;
    list->first.next = node;
    if (!node->next) {
        list->last = node;
    }
}

static inline void slist_add_tail(struct slist_node *node, struct slist_head *list)
{
    node->next = (struct slist_node *)0;
    list->last->next = node;
    list->last = node;
}

static inline void slist_add_after(struct slist_node *node, struct slist_node *prev, struct slist_head *list)
{
    node->next = prev->next;
    prev->next = node;
    if (!node->next) {
        list->last = node;
    }
}

static inline void slist_del_head(struct slist_head *list)
{
    list->first.next = list->first.next->next;
    if (!list->first.next) {
        list->last = &list->first;
    }
}

static inline void slist_del_after(struct slist_node *prev, struct slist_head *list)
{
    prev->next = prev->next->next;
    if (!prev->next) {
        list->last = prev;
    }
}

static inline bool slist_empty(struct slist_head *list)
{
    return !list->first.next;
}

static inline void __slist_splice(struct slist_head *list, struct slist_node *at, struct slist_head *head)
{
    list->last->next = at->next;
    at->next = list->first.next;
    if (!list->last->next) {
        head->last = list->last;
    }
}

static inline void slist_splice(struct slist_head *list, struct slist_node *at, struct slist_head *head)
{
    if (!slist_empty(list)) {
        __slist_splice(list, at, head);
    }
}

static inline void slist_splice_init(struct slist_head *list, struct slist_node *at, struct slist_head *head)
{
    if (!slist_empty(list)) {
        __slist_splice(list, at, head);
        INIT_SLIST_HEAD(list);
    }
}

#define slist_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define slist_for_each(pos, head) \
	for (pos = (head)->first.next; pos; pos = pos->next)

#define slist_for_each_safe(pos, prev, head) \
	for (prev = &(head)->first, pos = prev->next; pos; \
		 prev = prev->next == pos ? pos : prev, pos = prev->next)

#define slist_for_each_entry(pos, head, member) \
	for (pos = slist_entry((head)->first.next, typeof (*pos), member); \
		 &pos->member != (struct slist_node *)0; \
		 pos = slist_entry(pos->member.next, typeof (*pos), member))

#ifdef __cplusplus
}
#endif
#endif //JARVIS_C_LIST_H
