//
//  Cache.h
//  findit
//
//  Created by zuzu on 13-3-15.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef findit_Cache_h
#define findit_Cache_h

#include <vector>

template<typename T> class Cache
{
public:
    T* alloc()
    {
        if( cache.empty() )
            return new T();
        else
        {
            T* p = cache.back();
            cache.pop_back();
            return p;
        }
    }
    void free(T* p)
    {
        cache.push_back(p);
    }
    Cache()
    {
    }
    virtual ~Cache()
    {
        for(typename std::vector<T*>::iterator i=cache.begin();i!=cache.end();++i)
            delete *i;
        cache.clear();
    }
private:
    std::vector<T*> cache;
};

#endif
