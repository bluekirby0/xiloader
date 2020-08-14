#include "globals.h"

namespace xiloader
{
    void globals::setServerAddress(std::string addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        _serverAddress = std::string(addr);
    }

    void globals::getServerAddress(std::string* addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *addr = std::string(_serverAddress);
    }

    void globals::setHide(bool state)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        _hide = state;
    }

    void globals::getHide(bool* state)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *state = _hide;
    }
/*
    void globals::setNewServerAddress(DWORD addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        _newServerAddress = addr;
    }

    void globals::getNewServerAddress(DWORD *addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *addr = _newServerAddress;
    }

    void globals::setHairpinReturnAddress(DWORD addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        _hairpinReturnAddress = addr;
    }

    void globals::getHairpinReturnAddress(DWORD *addr)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *addr = _hairpinReturnAddress;
    }
*/
    void globals::setDrawDistance(float dist)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *_drawDistance = dist;
    }

    void globals::getDrawDistance(float* dist)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *dist = *_drawDistance;
    }

    float* globals::getDrawDistanceUnsafe()
    {
        return _drawDistance;
    }

    void globals::setMobDistance(float dist)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *_mobDistance = dist;
    }

    void globals::getMobDistance(float* dist)
    {
        const std::lock_guard<std::mutex> lock(_global_mutex);
        *dist = *_mobDistance;
    }

    float* globals::getMobDistanceUnsafe()
    {
        return _mobDistance;
    }
}
