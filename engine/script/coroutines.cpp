#include "script/coroutines.h"
#include "base/log.h"

#include <angelscript.h>

namespace nx
{
    void Coroutines::shutdown()
    {
        for (Item& item : m_items)
        {
            if (item.used)
            {
                free(item);
            }
            if (item.context)
            {
                item.context->Release();
                item.context = nullptr;
            }
        }
    }

    bool Coroutines::start(asIScriptFunction* function)
    {
        if (function == nullptr)
        {
            return false;
        }
        for (Item& item : m_items)
        {
            if (item.used)
            {
                continue;
            }
            item.context = item.context ? item.context : m_host->createContext();
            asIScriptFunction* target = function->GetDelegateFunction() ? function->GetDelegateFunction() : function;
            if (item.context == nullptr || item.context->Prepare(target) < 0)
            {
                break;
            }
            if (function->GetDelegateObject())
            {
                item.context->SetObject(function->GetDelegateObject());
            }
            item.function = function;
            item.group = m_group;
            item.waiting = 0;
            item.used = true;
            item.fresh = true;
            return true;
        }
        function->Release();
        return false;
    }

    bool Coroutines::wait(float seconds)
    {
        asIScriptContext* active = asGetActiveContext();
        for (Item& item : m_items)
        {
            if (item.used && item.context == active)
            {
                item.waiting = seconds;
                active->Suspend();
                return true;
            }
        }
        return false;
    }

    bool Coroutines::update(u32 group, float dt)
    {
        bool ok = true;
        for (Item& item : m_items)
        {
            if (!item.used || item.group != group)
            {
                continue;
            }
            if (item.fresh)
            {
                // Started during this update: its turn is the next one.
                item.fresh = false;
                continue;
            }
            item.waiting -= dt;
            if (item.waiting > 0)
            {
                continue;
            }
            bool suspended = false;
            const bool ran = m_host->resume(item.context, suspended);
            if (!ran)
            {
                ok = false;
            }
            if (!suspended)
            {
                free(item);
            }
        }
        return ok;
    }

    void Coroutines::free(Item& item)
    {
        item.used = false;
        item.context->Abort();
        item.context->Unprepare();
        if (item.function)
        {
            item.function->Release();
            item.function = nullptr;
        }
    }

    void Coroutines::stop(u32 group)
    {
        for (Item& item : m_items)
        {
            if (item.used && item.group == group)
            {
                free(item);
            }
        }
    }

    u32 Coroutines::count() const
    {
        u32 count = 0;
        for (const Item& item : m_items)
        {
            count += item.used;
        }
        return count;
    }
}
