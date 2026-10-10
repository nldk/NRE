#include "resourceManagement.h"
namespace NRE {
    void ResourceManager::destroyAll() {
        std::apply([](auto&... pool) { (pool.clear(), ...); }, pools);
    }
}
