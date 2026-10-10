#pragma once
#include <cstdint>
#include <tuple>
#include <vector>

#include "pipeline.h"
#include "shader.h"
#include "model.h"

namespace NRE {
    template<class T>
    class Pool {
        std::vector<T> items;
        std::vector<uint32_t> generations;
        std::vector<uint32_t>freeList;
    public:
        Handel<T> create(T value) {
            uint32_t i;
            if (!freeList.empty()) { i = freeList.back(); freeList.pop_back(); items[i] = std::move(value); }
            else { i = items.size(); items.push_back(std::move(value)); generations.push_back(0); }
            return { i, generations[i] };
        }
        T* get(Handel<T> h) {
            if (h.index >= items.size() || generations[h.index] != h.generation) return nullptr;
            return &items[h.index];
        }
        void destroy(Handel<T> h) {
            if (!get(h)) return;
            ++generations[h.index];
            freeList.push_back(h.index);
        }
        void clear() {
            items.clear();
            generations.clear();
            freeList.clear();
        }
    };
    class ResourceManager {
        static uint64_t nextid;
        std::tuple<Pool<Shader>, Pool<Pipeline>, Pool<Model>> pools;
    public:
        template<class T>
        Handel<T> create(T obj) { return std::get<Pool<T>>(pools).create(std::move(obj)); }

        template<class T>
        T* get(Handel<T> h) { return std::get<Pool<T>>(pools).get(h); }

        template<class T>
        void destroy(Handel<T> h) { std::get<Pool<T>>(pools).destroy(h); }

        void destroyAll();
        ~ResourceManager() {destroyAll();}
    };
}
