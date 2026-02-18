/*
The MIT License (MIT)

Copyright (c) 2013 Luis Ibanez

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

by @megantcs
*/

#ifndef MEGANTCS_EVENTBUS_HPP
#define MEGANTCS_EVENTBUS_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <vector>

#if __cplusplus < 201703L && !(defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
    #error "Upgrade to the 17th standard."
#endif


namespace std {
    struct null_mutex {
        void lock() {}
        void unlock() {}
        bool try_lock() { return true; }
    };
}

template<typename T, typename U>
class ComparableCallback {
    using raw_func = U(*)(T&);

    struct BaseInfo {
        void* instance;
        void* method_ptr;
        std::type_index instance_type;

        BaseInfo(void* inst, void* mptr, const std::type_info& type)
            : instance(inst), method_ptr(mptr), instance_type(type) {}

        bool operator==(const BaseInfo& other) const {
            return instance == other.instance &&
                   method_ptr == other.method_ptr &&
                   instance_type == other.instance_type;
        }
    };

    enum class CallbackType { Function, Method } callback_type;
    raw_func func_ptr;
    BaseInfo method_info;
    std::function<U(T&)> func;

public:
   constexpr ComparableCallback(raw_func ptr)
        : callback_type(CallbackType::Function),
          func_ptr(ptr),
          method_info(nullptr, nullptr, typeid(void)),
          func(ptr) {}

    template<typename C>
   constexpr ComparableCallback(U (C::*ptr)(T&), C* instance)
        : callback_type(CallbackType::Method),
          func_ptr(nullptr),
          method_info(instance, reinterpret_cast<void*&>(ptr), typeid(C)),
          func([instance, ptr](T& arg) { (instance->*ptr)(arg); }) {}

    template<typename C>
    constexpr ComparableCallback(U (C::*ptr)(T&) const, const C* instance)
        : callback_type(CallbackType::Method),
          func_ptr(nullptr),
          method_info(const_cast<C*>(instance), reinterpret_cast<void*&>(ptr), typeid(C)),
          func([instance, ptr](T& arg) { (instance->*ptr)(arg); }) {}

    constexpr ComparableCallback(const ComparableCallback& other)
        : callback_type(other.callback_type),
          func_ptr(other.func_ptr),
          method_info(other.method_info),
          func(other.func) {}

    constexpr ComparableCallback& operator=(const ComparableCallback& other) {
        if (this != &other) {
            callback_type = other.callback_type;
            func_ptr = other.func_ptr;
            method_info = other.method_info;
            func = other.func;
        }
        return *this;
    }

    constexpr ComparableCallback(ComparableCallback&& other) noexcept
        : callback_type(other.callback_type),
          func_ptr(other.func_ptr),
          method_info(std::move(other.method_info)),
          func(std::move(other.func)) {
        other.func_ptr = nullptr;
    }

    constexpr ComparableCallback& operator=(ComparableCallback&& other) noexcept {
        if (this != &other) {
            callback_type = other.callback_type;
            func_ptr = other.func_ptr;
            method_info = std::move(other.method_info);
            func = std::move(other.func);
            other.func_ptr = nullptr;
        }
        return *this;
    }

    constexpr void invoke(T &arg) {
        if (func) {
            func(arg);
        }
    }

    constexpr void release() {
        func = nullptr;
        func_ptr = nullptr;
    }

    constexpr bool operator==(const ComparableCallback &other) const {
        if (callback_type != other.callback_type) return false;

        if (callback_type == CallbackType::Function) {
            return func_ptr == other.func_ptr;
        }

        return method_info == other.method_info;
    }
};

template<typename EventType>
constexpr auto make_func(void (*func)(EventType&)) {
    return ComparableCallback<EventType, void>(func);
}

template<typename ClassType, typename EventType>
constexpr auto make_func(void (ClassType::*method)(EventType&), ClassType* instance) {
    return ComparableCallback<EventType, void>(method, instance);
}

template<typename ClassType, typename EventType>
constexpr auto make_const_func(void (ClassType::*method)(EventType&) const, const ClassType* instance) {
    return ComparableCallback<EventType, void>(method, instance);
}


enum class EventPriority
{
    VeryLow = 0x1B,
    Low = 0x2B,
    Default = 0x3B,
    High = 0x4B,
    VeryHigh = 0x5B
};

typedef unsigned char EventPriorityFlag;

class BaseEventHandler {
public:
    virtual ~BaseEventHandler() = default;
};

template <typename T, typename U, typename MutexPolicy>
class SimpleUserEventHandler : public BaseEventHandler {
    using t_callback = ComparableCallback<T, U>;
    using t_callbacks = std::vector<std::pair<t_callback, EventPriority>>;

    t_callbacks callbacks_;
    MutexPolicy mutex_;
public:
    constexpr void add_callback(t_callback callback, EventPriority priority = EventPriority::Default) {
        std::lock_guard<MutexPolicy> lock(mutex_);
        callbacks_.push_back(std::make_pair(callback, priority));

        std::sort(callbacks_.begin(), callbacks_.end(),
            [](const auto& a, const auto& b) {
                return static_cast<int>(a.second) > static_cast<int>(b.second);
            });
    }
    constexpr auto find_callback_it(t_callback callback)
    {
        std::lock_guard<MutexPolicy> lock(mutex_);
        for (auto it = callbacks_.begin(); it != callbacks_.end(); ++it) {
            if (it->first == callback) {
                return it;
            }
        }
        return callbacks_.end();
    }

    constexpr U invoke_all(T& arg)
    {
        std::lock_guard<MutexPolicy> lock(mutex_);
        if constexpr (std::is_same_v<U, void>)
        {
            for (auto &callback : callbacks_) {
                callback.first.invoke(arg);
            }
            return;
        }
        else {
            U u;
            for (auto &callback : callbacks_) {
                u = callback.first.invoke(arg);
            }
            return u;
        }
    }

    constexpr void delete_callback(t_callback callback) {
        std::lock_guard<MutexPolicy> lock(mutex_);
        if (auto it = find_callback_it(callback);
            it != callbacks_.end())
            callbacks_.erase(it);
    }

    constexpr auto get_callbacks() {
        return callbacks_;
    }
};

template <
    typename TypeMutex,
    typename TypeBaseEventHandler,
    template<typename, typename, typename> class TypeUserEventHandler>
class AbstractEventBus
{
protected:
    using MutexPolicy = TypeMutex;
    using AbstractEventHandler = std::unique_ptr<TypeBaseEventHandler>;

    template <typename TypeEvent>
    using UserEventHandler = TypeUserEventHandler<TypeEvent, void, TypeMutex>;
};

template <typename TypeMutex = std::mutex>
class EventBus
    : public AbstractEventBus<TypeMutex,
                                BaseEventHandler,
                                SimpleUserEventHandler>
{
    std::unordered_map<std::type_index,
                typename EventBus::AbstractEventHandler> handlers;

    template <typename EventType>
    using Callback = ComparableCallback<EventType, void>;

    mutable typename EventBus::MutexPolicy mutex_;
public:
    template<typename TypeEvent>
    void Subscribe(Callback<TypeEvent> callback, EventPriority priority = EventPriority::Default);

    template<typename TypeEvent>
    bool Unsubscribe(Callback<TypeEvent> callback);

    template<typename TypeEvent>
    bool Publish(TypeEvent& arg);
};

template<typename TypeMutex>
template<typename TypeEvent>
void EventBus<TypeMutex>::Subscribe(Callback<TypeEvent> callback, EventPriority priority) {
    std::lock_guard<TypeMutex> lock(mutex_);
    const auto index = std::type_index(typeid(TypeEvent));

    if (handlers.find(index) == handlers.end()) {
        handlers[index] = std::make_unique<typename EventBus::template
                        UserEventHandler<TypeEvent>>();
    }

    static_cast<typename EventBus::template UserEventHandler<TypeEvent>*>(
                    handlers[index].get())->add_callback(callback, priority);
}

template<typename TypeMutex>
template<typename TypeEvent>
bool EventBus<TypeMutex>::Unsubscribe(Callback<TypeEvent> callback) {
    std::lock_guard<TypeMutex> lock(mutex_);
    const auto index = std::type_index(typeid(TypeEvent));
    if (handlers.find(index) != handlers.end()) {
        static_cast<typename EventBus::template UserEventHandler<TypeEvent>*>(
                    handlers[index].get())->delete_callback(callback);
        return true;
    }
    return false;
}

template<typename TypeMutex>
template<typename TypeEvent>
bool EventBus<TypeMutex>::Publish(TypeEvent& arg) {
    using CallbackPair = std::pair<Callback<TypeEvent>, EventPriority>;
    std::vector<CallbackPair> callbacks_copy;

    {
        std::lock_guard<TypeMutex> lock(mutex_);
        auto it = handlers.find(typeid(TypeEvent));
        if (it == handlers.end()) return false;

        auto* handler = static_cast<typename EventBus::template UserEventHandler<TypeEvent>*>(
                    it->second.get());
        callbacks_copy = handler->get_callbacks();
    }

    for (auto& [callback, priority] : callbacks_copy) {
        callback.invoke(arg);
    }
    return true;
}

#endif //MEGANTCS_EVENTBUS_HPP
