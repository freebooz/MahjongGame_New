#include "Game/GuiyangClientControllerBridge.h"

namespace
{
    FGuiyangClientBridgeFactory GClientBridgeFactory;
}

void FGuiyangClientBridgeRegistry::Register(FGuiyangClientBridgeFactory Factory)
{
    GClientBridgeFactory = MoveTemp(Factory);
}

void FGuiyangClientBridgeRegistry::Unregister()
{
    GClientBridgeFactory = nullptr;
}

UObject* FGuiyangClientBridgeRegistry::Create(AGuiyangMahjongPlayerController& Controller)
{
    return GClientBridgeFactory ? GClientBridgeFactory(Controller) : nullptr;
}
