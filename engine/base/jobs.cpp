#include "base/thread.h"

namespace nx
{
    bool JobQueue::start()
    {
        m_stopping = false;
        m_thread = startThread(run, this, ThreadPriority::Normal);
        m_started = true;
        return true;
    }

    void JobQueue::stop()
    {
        if (!m_started)
        {
            return;
        }
        m_started = false;
        if (m_thread)
        {
            {
                Lock lock(m_mutex);
                m_stopping = true;
            }
            m_ready.post();
            joinThread(m_thread);
            m_thread = {};
        }
    }

    bool JobQueue::add(ThreadFunction function, void* user)
    {
        if (!m_thread)
        {
            function(user);
            return true;
        }
        {
            Lock lock(m_mutex);
            if (m_count == kCapacity)
            {
                return false;
            }
            m_jobs[(m_first + m_count) % kCapacity] = { function, user };
            m_count++;
        }
        m_ready.post();
        return true;
    }

    void JobQueue::run(void* user)
    {
        JobQueue& queue = *static_cast<JobQueue*>(user);
        for (;;)
        {
            queue.m_ready.wait();
            Job job = {};
            {
                Lock lock(queue.m_mutex);
                if (queue.m_count)
                {
                    job = queue.m_jobs[queue.m_first];
                    queue.m_first = (queue.m_first + 1) % kCapacity;
                    queue.m_count--;
                }
                else if (queue.m_stopping)
                {
                    return;
                }
            }
            if (job.function)
            {
                job.function(job.user);
            }
        }
    }
}
