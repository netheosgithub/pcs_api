/**
 * Copyright (c) 2014 Netheos (http://www.netheos.net)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PCS_API_INTERNAL_OBJECT_POOL_H_
#define INCLUDE_PCS_API_INTERNAL_OBJECT_POOL_H_

#include <functional>
#include <list>
#include <mutex>

#include "pcs_api/internal/logger.h"

namespace pcs_api {

/**
 * \brief A simple class for objects pooling.
 *
 * Object is taken from the pool with method Get(), and given back with Put().
 * When pool is empty, new objects are allocated with the given arbitrary
 * function.
 * When pool is destroyed, all objects are destroyed with the given arbitrary
 * function.
 *
 * This class is thread safe.
 */
template<class T>
class ObjectPool {
 public:
    ObjectPool(std::function<T*()> create_object_function,
               std::function<void(T*)> delete_object_function) :
        create_function_(create_object_function),
        delete_function_(delete_object_function) {
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    /**
     * Destroy pool and any objects that are currently pooled
     */
    ~ObjectPool() {
        LOG_TRACE << "Pool destructor will delete "
                  << pool_.size()
                  << " pooled client(s)";
        for (T* p : pool_) {
            delete_function_(p);
        }
    }

    /**
     * Get an object, either from pool or by constructing a new object.
     */
    T* Get() {
        std::lock_guard<std::mutex> pool_lock_guard(this->pool_lock_mutex_);
        if (pool_.empty()) {
            LOG_TRACE << "No client in pool: creating one";
            return create_function_();
        } else {
            // return last object from our pool:
            LOG_TRACE << "Getting client from pool";
            T* obj = pool_.back();
            pool_.pop_back();
            return obj;
        }
    }

    /**
     * Return an object to this pool
     */
    void Put(T *obj) {
        LOG_TRACE << "Putting client back in pool";
        std::lock_guard<std::mutex> pool_lock_guard(this->pool_lock_mutex_);
        pool_.push_back(obj);
    }

 protected:
    /**
     * Objects ready to be taken
     */
    std::list<T*> pool_;
    /**
     * function to create an object
     */
    std::function<T*()> create_function_;
    /**
     * function to delete an object
     */
    std::function<void(T*)> delete_function_;
    /**
     * mutex and lock for pool_ access
     */
    std::mutex pool_lock_mutex_;
};

}  // namespace pcs_api

#endif  // INCLUDE_PCS_API_INTERNAL_OBJECT_POOL_H_
