#pragma once

#include <deque>

namespace Elixir
{
    struct SDeletionQueue
    {
        std::deque<std::function<void()>> Deleters;

        void Push(std::function<void()>&& fn)
        {
            Deletors.push_back(fn);
        }

        void Flush()
        {
            // Reverse iterate the deletion queue to execute in the correct order.
            for (auto it = Deletors.rbegin(); it != Deletors.rend(); ++it)
            {
                (*it)();
            }

            Deletors.clear();
        }
    };
}