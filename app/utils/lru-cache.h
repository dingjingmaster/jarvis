//
// Created by dingjing on 8/17/22.
//

#ifndef JARVIS_LRU_CACHE_H
#define JARVIS_LRU_CACHE_H

#include <assert.h>

#include "../core/c-list.h"
#include "../core/rb-tree.h"

// RAII: NO. Release ref by LRUCache::release
// Thread safety: NO.
// DONOT change value by handler, use Cache::put instead
template<typename KEY, typename VALUE>
class LRUHandle
{
    template<typename, typename, class> friend class LRUCache;
private:
    LRUHandle(const KEY& k, const VALUE& v)
        : mValue(v), mKey(k)
    {
    }


public:
    VALUE                       mValue;

private:
    KEY                         mKey;
    struct list_head            mList;
    RBNode                      mRb;
    bool                        mInCache;
    int                         mRef;
};

// RAII: NO. Release ref by LRUCache::release
// Define ValueDeleter(VALUE& v) for value deleter
// Thread safety: NO
// Make sure KEY operator< usable
template<typename KEY, typename VALUE, class ValueDeleter>
class LRUCache
{
protected:
    typedef LRUHandle<KEY, VALUE>			Handle;

public:
    LRUCache()
    {
        INIT_LIST_HEAD(&mNotUse);
        INIT_LIST_HEAD(&mInUse);
        mCacheMap.rbNode = NULL;
        mMaxSize = 0;
        mSize = 0;
    }

    ~LRUCache()
    {
        struct list_head *pos, *tmp;
        Handle *e;

        // Error if caller has an unreleased handle
        assert(list_empty(&mInUse));
        list_for_each_safe(pos, tmp, &mNotUse) {
            e = list_entry(pos, Handle, list);
            assert(e->mInCache);
            e->mInCache = false;
            assert(e->mRef == 1);// Invariant for not_use_ list.
            this->unref(e);
        }
    }

    // default max_size=0 means no-limit cache
    // max_size means max cache number of key-value pairs
    void set_max_size(size_t max_size)
    {
        mMaxSize = max_size;
    }

    // Remove all cache that are not actively in use.
    void prune()
    {
        struct list_head *pos, *tmp;
        Handle *e;

        list_for_each_safe(pos, tmp, &mNotUse)
        {
            e = list_entry(&pos, Handle, list);
            assert(e->mRef == 1);
            rb_erase(&e->rb);
            this->erase_node(e);
        }
    }

    // release handle by get/put
    void release(const Handle *handle)
    {
        this->unref(const_cast<Handle *>(handle));
    }

    // get handler
    // Need call release when handle no longer needed
    const Handle *get(const KEY& key)
    {
        RBNode*p = mCacheMap.rbNode;
        Handle *bound = NULL;
        Handle *e;

        while (p)
        {
            e = RB_ENTRY (p, Handle, mRb);
            if (!(e->mKey < key))
            {
                bound = e;
                p = p->rbLeft;
            }
            else
                p = p->rbRight;
        }

        if (bound && !(key < bound->mKey))
        {
            this->mRef(bound);
            return bound;
        }

        return NULL;
    }

    // put copy
    // Need call release when handle no longer needed
    const Handle *put(const KEY& key, VALUE value)
    {
        RBNode**p = &mCacheMap.rbNode;
        RBNode* parent = NULL;
        Handle *bound = NULL;
        Handle *e;

        while (*p)
        {
            parent = *p;
            e = RB_ENTRY(*p, Handle, mRb);
            if (!(e->mKey < key))
            {
                bound = e;
                p = &(*p)->rbLeft;
            }
            else
                p = &(*p)->rbRight;
        }

        e = new Handle(key, value);
        e->mInCache = true;
        e->mRef = 2;
        list_add_tail(&e->mList, &mInUse);
        mSize++;

        if (bound && !(key < bound->mKey))
        {
            rb_replace_node(&bound->mRb, &e->mRb, &mCacheMap);
            this->erase_node(bound);
        }
        else
        {
            rb_link_node(&e->mRb, parent, p);
            rb_insert_color(&e->mRb, &mCacheMap);
        }

        if (mMaxSize > 0)
        {
            while (mSize > mMaxSize && !list_empty(&mNotUse))
            {
                Handle *tmp = list_entry(mNotUse.next, Handle, mList);
                assert(tmp->mRef == 1);
                rb_erase(&tmp->mRb, &mCacheMap);
                this->erase_node(tmp);
            }
        }

        return e;
    }

    // delete from cache, deleter delay called when all inuse-handle release.
    void del(const KEY& key)
    {
        Handle *e = const_cast<Handle *>(this->get(key));

        if (e)
        {
            this->unref(e);
            rb_erase(&e->mRb, &mCacheMap);
            this->erase_node(e);
        }
    }

private:
    void ref(Handle *e)
    {
        if (e->mInCache && e->mRef == 1)
            list_move_tail(&e->mList, &mInUse);

        e->mRef++;
    }

    void unref(Handle *e)
    {
        assert(e->mRef > 0);
        if (--e->mRef == 0)
        {
            assert(!e->mInCache);
            mValueDeleter(e->value);
            delete e;
        }
        else if (e->mInCache && e->mRef == 1)
            list_move_tail(&e->list, &mNotUse);
    }

    void erase_node(Handle *e)
    {
        assert(e->mInCache);
        list_del(&e->list);
        e->mInCache = false;
        --mSize;
        this->unref(e);
    }


private:
    size_t                          mMaxSize;
    size_t                          mSize;

    RBRoot                          mCacheMap;
    struct list_head                mNotUse;
    struct list_head                mInUse;

    ValueDeleter                    mValueDeleter;
};

#endif //JARVIS_LRU_CACHE_H
