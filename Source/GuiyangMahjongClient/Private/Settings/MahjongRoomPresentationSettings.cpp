#include "Settings/MahjongRoomPresentationSettings.h"

UMahjongRoomPresentationSettings::UMahjongRoomPresentationSettings()
{
    // The concrete class is intentionally supplied only by client platform
    // configs. A native default here becomes a Cook soft reference while the
    // Editor hosts a Dedicated Server cook and contaminates the server package.
    PresentationClass.Reset();
}
