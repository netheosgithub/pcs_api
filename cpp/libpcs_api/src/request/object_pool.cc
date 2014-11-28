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

/*
*/

#include "pcs_api/internal/object_pool.h"
#include "pcs_api/internal/logger.h"

namespace pcs_api {

template<class T>
ObjectPool::ObjectPool(std::function<T*()> create_object_function,
                       std::function<void(T*)> delete_object_function) :
    create_function_(create_object_function),
    delete_function_(delete_object_function) {
}

template<class T>
T* ObjectPool<T>::Get() {
    if (pool_.empty()) {
        return create_function_();
    } else {
        // return last object from our pool:
        T* obj = pool_.back();
        pool_.pop_back();
        return obj;
    }
}

template<class T>
void ObjectPool::Put(T *obj) {
    pool_.push_back(obj);
}


}  // namespace pcs_api
