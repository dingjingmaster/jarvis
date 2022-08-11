//
// Created by dingjing on 8/11/22.
//

#ifndef JARVIS_RB_TREE_H
#define JARVIS_RB_TREE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _RBRoot          RBRoot;
typedef struct _RBNode          RBNode;

#pragma pack(1)

struct _RBNode
{
    char                        rbColor;
    RBNode*                     rbParent;
    RBNode*                     rbLeft;
    RBNode*                     rbRight;

#define RB_RED                  0
#define RB_BLACK                1
};
#pragma pack()


struct _RBRoot
{
    RBNode*                     rbNode;
};

#define RB_ROOT (RBRoot)                {(RBNode*)0,}
#define RB_ENTRY(ptr, type, member)     ((type*)((char*)(ptr)-(unsigned long)(&((type*)0)->member)))

bool rb_is_black (RBNode* node);
void rb_erase (RBNode* node, RBRoot* root);
void rb_insert_color (RBNode* node, RBRoot* root);

//
RBNode* rb_next     (RBNode*);
RBNode* rb_prev     (RBNode*);
RBNode* rb_first    (RBRoot*);
RBNode* rb_last     (RBRoot*);

//
void rb_link_node (RBNode* node, RBNode* parent, RBNode** link);
void rb_replace_node (RBNode* victim, RBNode* newNode, RBRoot* root);

#ifdef __cplusplus
};
#endif
#endif //JARVIS_RB_TREE_H
