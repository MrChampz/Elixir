#pragma once

#include <deque>

namespace Elixir
{
    struct SDeletionQueue
    {
        std::deque<std::function<void()>> Deleters;

        void Push(std::function<void()>&& fn)
        {
            Deleters.push_back(fn);
        }

        void Flush()
        {
            // Reverse iterate the deletion queue to execute in the correct order.
            for (auto it = Deleters.rbegin(); it != Deleters.rend(); ++it)
            {
                (*it)();
            }

            Deleters.clear();
        }
    };
}