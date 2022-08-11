//
// Created by dingjing on 8/11/22.
//

#include "rb-tree.h"

static void __rb_rotate_left(RBNode* node, RBRoot* root)
{
    RBNode *right = node->rbRight;

    if ((node->rbRight = right->rbLeft)) {
        right->rbLeft->rbParent = node;
    }
    right->rbLeft = node;

    if ((right->rbParent = node->rbParent)) {
        if (node == node->rbParent->rbLeft) {
            node->rbParent->rbLeft = right;
        } else {
            node->rbParent->rbRight = right;
        }
    } else {
        root->rbNode = right;
    }

    node->rbParent = right;
}

static void __rb_rotate_right(RBNode *node, RBRoot *root)
{
    RBNode *left = node->rbLeft;

    if ((node->rbLeft = left->rbRight)) {
        left->rbRight->rbParent = node;
    }
    left->rbRight = node;

    if ((left->rbParent = node->rbParent)) {
        if (node == node->rbParent->rbRight) {
            node->rbParent->rbRight = left;
        } else {
            node->rbParent->rbLeft = left;
        }
    } else {
        root->rbNode = left;
    }
    node->rbParent = left;
}

void rb_insert_color(RBNode *node, RBRoot *root)
{
    RBNode *parent, *gparent;

    while ((parent = node->rbParent) && parent->rbColor == RB_RED) {
        gparent = parent->rbParent;
        if (parent == gparent->rbLeft) {
            {
                register RBNode *uncle = gparent->rbRight;
                if (uncle && uncle->rbColor == RB_RED) {
                    uncle->rbColor = RB_BLACK;
                    parent->rbColor = RB_BLACK;
                    gparent->rbColor = RB_RED;
                    node = gparent;
                    continue;
                }
            }

            if (parent->rbRight == node) {
                register RBNode *tmp;
                __rb_rotate_left(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->rbColor = RB_BLACK;
            gparent->rbColor = RB_RED;
            __rb_rotate_right(gparent, root);
        } else {
            {
                register RBNode *uncle = gparent->rbLeft;
                if (uncle && uncle->rbColor == RB_RED) {
                    uncle->rbColor = RB_BLACK;
                    parent->rbColor = RB_BLACK;
                    gparent->rbColor = RB_RED;
                    node = gparent;
                    continue;
                }
            }

            if (parent->rbLeft == node) {
                register RBNode *tmp;
                __rb_rotate_right(parent, root);
                tmp = parent;
                parent = node;
                node = tmp;
            }

            parent->rbColor = RB_BLACK;
            gparent->rbColor = RB_RED;
            __rb_rotate_left(gparent, root);
        }
    }

    root->rbNode->rbColor = RB_BLACK;
}

static void __rb_erase_color(RBNode *node, RBNode *parent, RBRoot *root)
{
    RBNode *other;

    while ((!node || node->rbColor == RB_BLACK) && node != root->rbNode) {
        if (parent->rbLeft == node) {
            other = parent->rbRight;
            if (other->rbColor == RB_RED) {
                other->rbColor = RB_BLACK;
                parent->rbColor = RB_RED;
                __rb_rotate_left(parent, root);
                other = parent->rbRight;
            }
            if ((!other->rbLeft || other->rbLeft->rbColor == RB_BLACK)
                && (!other->rbRight || other->rbRight->rbColor == RB_BLACK)) {
                other->rbColor = RB_RED;
                node = parent;
                parent = node->rbParent;
            } else {
                if (!other->rbRight || other->rbRight->rbColor == RB_BLACK) {
                    register RBNode *o_left;
                    if ((o_left = other->rbLeft)) {
                        o_left->rbColor = RB_BLACK;
                    }
                    other->rbColor = RB_RED;
                    __rb_rotate_right(other, root);
                    other = parent->rbRight;
                }
                other->rbColor = parent->rbColor;
                parent->rbColor = RB_BLACK;
                if (other->rbRight) {
                    other->rbRight->rbColor = RB_BLACK;
                }
                __rb_rotate_left(parent, root);
                node = root->rbNode;
                break;
            }
        } else {
            other = parent->rbLeft;
            if (other->rbColor == RB_RED) {
                other->rbColor = RB_BLACK;
                parent->rbColor = RB_RED;
                __rb_rotate_right(parent, root);
                other = parent->rbLeft;
            }
            if ((!other->rbLeft || other->rbLeft->rbColor == RB_BLACK)
                && (!other->rbRight || other->rbRight->rbColor == RB_BLACK)) {
                other->rbColor = RB_RED;
                node = parent;
                parent = node->rbParent;
            } else {
                if (!other->rbLeft || other->rbLeft->rbColor == RB_BLACK) {
                    register RBNode *o_right;
                    if ((o_right = other->rbRight)) {
                        o_right->rbColor = RB_BLACK;
                    }
                    other->rbColor = RB_RED;
                    __rb_rotate_left(other, root);
                    other = parent->rbLeft;
                }
                other->rbColor = parent->rbColor;
                parent->rbColor = RB_BLACK;
                if (other->rbLeft) {
                    other->rbLeft->rbColor = RB_BLACK;
                }
                __rb_rotate_right(parent, root);
                node = root->rbNode;
                break;
            }
        }
    }
    if (node) {
        node->rbColor = RB_BLACK;
    }
}

bool rb_is_black (RBNode* node)
{
    return (node->rbColor == RB_BLACK) ? true : false;
}

void rb_erase(RBNode *node, RBRoot *root)
{
    RBNode *child, *parent;
    int color;

    if (!node->rbLeft) {
        child = node->rbRight;
    } else if (!node->rbRight) {
        child = node->rbLeft;
    } else {
        RBNode *old = node, *left;

        node = node->rbRight;
        while ((left = node->rbLeft)) {
            node = left;
        }
        child = node->rbRight;
        parent = node->rbParent;
        color = node->rbColor;

        if (child) {
            child->rbParent = parent;
        }
        if (parent) {
            if (parent->rbLeft == node) {
                parent->rbLeft = child;
            } else {
                parent->rbRight = child;
            }
        } else {
            root->rbNode = child;
        }

        if (node->rbParent == old) {
            parent = node;
        }
        node->rbParent = old->rbParent;
        node->rbColor = old->rbColor;
        node->rbRight = old->rbRight;
        node->rbLeft = old->rbLeft;

        if (old->rbParent) {
            if (old->rbParent->rbLeft == old) {
                old->rbParent->rbLeft = node;
            } else {
                old->rbParent->rbRight = node;
            }
        } else {
            root->rbNode = node;
        }

        old->rbLeft->rbParent = node;
        if (old->rbRight) {
            old->rbRight->rbParent = node;
        }
        goto color;
    }

    parent = node->rbParent;
    color = node->rbColor;

    if (child) {
        child->rbParent = parent;
    }
    if (parent) {
        if (parent->rbLeft == node) {
            parent->rbLeft = child;
        } else {
            parent->rbRight = child;
        }
    } else {
        root->rbNode = child;
    }

color:
    if (color == RB_BLACK) {
        __rb_erase_color(child, parent, root);
    }
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
RBNode *rb_first(RBRoot *root)
{
    RBNode *n;

    n = root->rbNode;
    if (!n) {
        return (RBNode *)0;
    }
    while (n->rbLeft) {
        n = n->rbLeft;
    }

    return n;
}

RBNode *rb_last(RBRoot *root)
{
    RBNode *n;

    n = root->rbNode;
    if (!n) {
        return (RBNode *)0;
    }
    while (n->rbRight) {
        n = n->rbRight;
    }

    return n;
}

RBNode *rb_next(RBNode *node)
{
    /* If we have a right-hand child, go down and then left as far
       as we can. */
    if (node->rbRight) {
        node = node->rbRight;
        while (node->rbLeft) {
            node = node->rbLeft;
        }

        return node;
    }

    /* No right-hand children.  Everything down and left is
       smaller than us, so any 'next' node must be in the general
       direction of our parent. Go up the tree; any time the
       ancestor is a right-hand child of its parent, keep going
       up. First time it's a left-hand child of its parent, said
       parent is our 'next' node. */
    while (node->rbParent && node == node->rbParent->rbRight) {
        node = node->rbParent;
    }

    return node->rbParent;
}

RBNode *rb_prev(RBNode *node)
{
    /* If we have a left-hand child, go down and then right as far
       as we can. */
    if (node->rbLeft) {
        node = node->rbLeft;
        while (node->rbRight) {
            node = node->rbRight;
        }

        return node;
    }

    /* No left-hand children. Go up till we find an ancestor which
       is a right-hand child of its parent */
    while (node->rbParent && node == node->rbParent->rbLeft)
        node = node->rbParent;

    return node->rbParent;
}

void rb_replace_node(RBNode *victim, RBNode *newnode, RBRoot *root)
{
    RBNode *parent = victim->rbParent;

    /* Set the surrounding nodes to point to the replacement */
    if (parent) {
        if (victim == parent->rbLeft) {
            parent->rbLeft = newnode;
        } else {
            parent->rbRight = newnode;
        }
    } else {
        root->rbNode = newnode;
    }
    if (victim->rbLeft) {
        victim->rbLeft->rbParent = newnode;
    }
    if (victim->rbRight) {
        victim->rbRight->rbParent = newnode;
    }

    /* Copy the pointers/colour from the victim to the replacement */
    *newnode = *victim;
}

void rb_link_node (RBNode* node, RBNode* parent, RBNode** link)
{
    node->rbParent = parent;
    node->rbColor = RB_RED;
    node->rbLeft = node->rbRight = (RBNode*)0;

    *link = node;
}