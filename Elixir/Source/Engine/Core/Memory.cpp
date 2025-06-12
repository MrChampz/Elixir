#include "Memory.h"

namespace Elixir
{
    Scope<Malloc> Memory::s_Malloc = CreateScope<SystemMalloc>();
}