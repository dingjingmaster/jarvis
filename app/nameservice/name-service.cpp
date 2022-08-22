//
// Created by dingjing on 8/19/22.
//

#include "name-service.h"

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

struct _NSPolicyEntry
{
    RBNode                          rb;
    NSPolicy*                       policy;
    char                            name[1];
};

int NameService::addPolicy(const char *name, NSPolicy *policy)
{
    RBNode** p = &mRoot.rbNode;
    RBNode* parent = NULL;
    NSPolicyEntry *entry;
    int n, ret = -1;

    pthread_rwlock_wrlock(&mRWLock);
    while (*p) {
        parent = *p;
        entry = RB_ENTRY(*p, NSPolicyEntry, rb);
        n = strcasecmp(name, entry->name);
        if (n < 0)
            p = &(*p)->rbLeft;
        else if (n > 0)
            p = &(*p)->rbRight;
        else
            break;
    }

    if (!*p) {
        size_t len = strlen(name);
        size_t size = offsetof(NSPolicyEntry, name) + len + 1;

        entry = (NSPolicyEntry *)malloc(size);
        if (entry) {
            memcpy(entry->name, name, len + 1);
            entry->policy = policy;
            rb_link_node(&entry->rb, parent, p);
            rb_insert_color(&entry->rb, &mRoot);
            ret = 0;
        }
    } else {
        errno = EEXIST;
    }

    pthread_rwlock_unlock(&mRWLock);

    return ret;
}

inline NSPolicyEntry *NameService::getPolicyEntry(const char *name)
{
    RBNode* p = mRoot.rbNode;
    NSPolicyEntry *entry;
    int n;

    while (p) {
        entry = RB_ENTRY(p, NSPolicyEntry, rb);
        n = strcasecmp(name, entry->name);
        if (n < 0)
            p = p->rbLeft;
        else if (n > 0)
            p = p->rbRight;
        else
            return entry;
    }

    return NULL;
}

NSPolicy* NameService::getPolicy(const char *name)
{
    NSPolicy *policy = mDefaultPolicy;
    NSPolicyEntry *entry;

    if (mRoot.rbNode) {
        pthread_rwlock_rdlock(&mRWLock);
        entry = getPolicyEntry(name);
        if (entry)
            policy = entry->policy;

        pthread_rwlock_unlock(&mRWLock);
    }

    return policy;
}

NSPolicy* NameService::delPolicy(const char *name)
{
    NSPolicy *policy = NULL;
    NSPolicyEntry *entry;

    pthread_rwlock_wrlock(&mRWLock);
    entry = getPolicyEntry(name);
    if (entry)
    {
        policy = entry->policy;
        rb_erase(&entry->rb, &mRoot);
    }

    pthread_rwlock_unlock(&mRWLock);
    free(entry);

    return policy;
}

NameService::~NameService()
{
    NSPolicyEntry *entry;

    while (mRoot.rbNode) {
        entry = RB_ENTRY(mRoot.rbNode, NSPolicyEntry, rb);
        rb_erase(&entry->rb, &mRoot);
        free(entry);
    }
}

