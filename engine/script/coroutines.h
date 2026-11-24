#pragma once

#include "script/script_host.h"

namespace nx
{
    // Script functions that can wait: for a time, or for the next update. They run one after another
    // during update, never at the same time.
    class Coroutines : NonCopyable
    {
    public:
        ~Coroutines() { shutdown(); }

        void init(ScriptHost& host) { m_host = &host; }
        void shutdown();

        // New ones belong to this group.
        void setGroup(u32 group) { m_group = group; }

        // Runs from the next update. Takes over the caller's reference to the function.
        bool start(asIScriptFunction* function);

        // From inside a coroutine. False if it isn't one.
        bool wait(float seconds);

        // Runs each of a group's coroutines up to its next wait. False if one failed; the host has
        // the error.
        bool update(u32 group, float dt);
        void stop(u32 group);

        u32 count() const;

    private:
        static constexpr u32 kCapacity = 64;

        struct Item
        {
            asIScriptContext* context = nullptr;
            asIScriptFunction* function = nullptr;
            u32 group = 0;
            float waiting = 0;
            bool used = false;
            bool fresh = false;
        };

        void free(Item& item);

        ScriptHost* m_host = nullptr;
        Item m_items[kCapacity];
        u32 m_group = 0;
    };
}
