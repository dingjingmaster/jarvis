//
// Created by dingjing on 8/11/22.
//

#include "../app/core/rb-tree.h"
#include <gtest/gtest.h>

typedef struct _MyRBTreeNode        MyRBTreeNode;

struct _MyRBTreeNode
{
    RBNode          rbNode;

    int             v;
};


// 创建节点
MyRBTreeNode* create_node (int v)
{
    MyRBTreeNode* node = (MyRBTreeNode*) malloc (sizeof (MyRBTreeNode));
    if (NULL == node) {
        return NULL;
    }

    memset (node, 0, sizeof (MyRBTreeNode));

    node->v = v;

    return node;
}

// 查找节点
MyRBTreeNode* find_node (RBRoot* root, int v)
{
    RBNode* node = root->rbNode;
    MyRBTreeNode* target = NULL;

    while (node) {
        target = RB_ENTRY(node, MyRBTreeNode, rbNode);
        int res = target->v - v;

        if (res < 0) {
            node = node->rbLeft;
        } else if (res > 0) {
            node = node->rbRight;
        } else {
            return target;
        }
    }

    return NULL;
}

int insert_node (RBRoot* root, int v)
{
    RBNode**        new1 = &(root->rbNode);
    RBNode*         parent = NULL;
    MyRBTreeNode*   target = NULL;

    int res = 0;

    while (*new1) {
        parent = *new1;
        target = RB_ENTRY(*new1, MyRBTreeNode, rbNode);
        res = target->v - v;
        if (res < 0) {
            new1 = &((*new1)->rbLeft);
        } else if (res > 0) {
            new1 = &((*new1)->rbRight);
        } else {
            return true;
        }
    }

    target = create_node(v);
    if (NULL == target) {
        return false;
    }

    rb_link_node(&target->rbNode, parent, new1);

    rb_insert_color(&target->rbNode, root);

    return true;
}

// 删除单个节点
void delete_node (RBRoot* root, int v)
{
    MyRBTreeNode* node = find_node(root, v);
    if (NULL != node) {
        rb_erase(&node->rbNode, root);
        free(node);
    }
}

// 遍历
static void traverse (RBRoot* root)
{
    RBNode*     node;
    int         index = 0;

    for (node = rb_first(root); node; node = rb_next(node)) {
        MyRBTreeNode* target = RB_ENTRY(node, MyRBTreeNode, rbNode);
        printf("[%d] %p key: %d, pc: 0x%x [ %c ], rb_left: %p, rb_right: %p\n",
                index, target, target->v, target->rbNode.rbParent, rb_is_black(&(target->rbNode)) ? 'B' : 'R',
                target->rbNode.rbLeft, target->rbNode.rbRight);
        ++index;
    }
}

// test

TEST(RB_TREE, RB_INSERT) {
    static RBRoot root = RB_ROOT;
    for (int i = 0; i < 10; ++i) {
        printf("----------------- BEGIN %d --------------------\n", i);
        ASSERT_TRUE(insert_node(&root, i));
        traverse(&root);
        printf("----------------- END --------------------\n", i);
    }
};

TEST(RB_TREE, RB_FIND) {
    static RBRoot root = RB_ROOT;
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(insert_node(&root, i));
    }

    for (int i = 0; i < 10; ++i) {
        MyRBTreeNode* node = find_node(&root, i);
        ASSERT_TRUE(i == node->v);
    }
}

TEST(RB_TREE, RB_DELETE) {
    static RBRoot root = RB_ROOT;
    for (int i = 0; i < 10; ++i) {
        ASSERT_TRUE(insert_node(&root, i));
    }

    for (int i = 0; i < 10; ++i) {
        delete_node(&root, i);
    }

    for (int i = 0; i < 10; ++i) {
        MyRBTreeNode* node = find_node(&root, i);
        ASSERT_TRUE(NULL == node);
    }
}