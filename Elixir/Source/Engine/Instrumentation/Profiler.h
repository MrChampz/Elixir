#pragma once

#ifdef EE_PROFILE
    #define EE_PROFILE_FRAME_MARK() FrameMark;
    #define EE_PROFILE_FRAME_MARK_NAMED(name) FrameMarkNamed(name);
    #define EE_PROFILE_FRAME_MARK_START(name) FrameMarkStart(name);
    #define EE_PROFILE_FRAME_MARK_END(name) FrameMarkEnd(name);
    #define EE_PROFILE_ZONE_SCOPED() ZoneScoped;
    #define EE_PROFILE_ZONE_SCOPED_NAMED(name) ZoneScopedN(name);
    #define EE_PROFILE_ZONE_SCOPED_COLOR(name, color) ZoneScopedNC(name, color);
    #define EE_PROFILE_LOG(text, size) TracyMessage(text, size)
#else
    #define EE_PROFILE_FRAME_MARK()
    #define EE_PROFILE_FRAME_MARK_NAMED(name)
    #define EE_PROFILE_FRAME_MARK_START(name)
    #define EE_PROFILE_FRAME_MARK_END(name)
    #define EE_PROFILE_ZONE_SCOPED()
    #define EE_PROFILE_ZONE_SCOPED_NAMED(name)
    #define EE_PROFILE_ZONE_SCOPED_COLOR(name, color)
    #define EE_PROFILE_LOG(text, size)
#endif