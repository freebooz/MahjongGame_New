#include "Modules/ModuleManager.h"

#include "Game/GuiyangClientControllerBridge.h"
#include "Game/GuiyangClientControllerBridgeImpl.h"
#include "Game/GuiyangMahjongPlayerController.h"

class FGuiyangMahjongClientModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGuiyangClientBridgeRegistry::Register([](AGuiyangMahjongPlayerController& Controller) -> UObject*
        {
            return NewObject<UGuiyangClientControllerBridgeImpl>(&Controller, NAME_None, RF_Transient);
        });
    }

    virtual void ShutdownModule() override
    {
        FGuiyangClientBridgeRegistry::Unregister();
    }
};

IMPLEMENT_MODULE(FGuiyangMahjongClientModule, GuiyangMahjongClient);
