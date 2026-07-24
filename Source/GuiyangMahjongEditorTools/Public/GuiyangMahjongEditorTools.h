#pragma once

#include "Modules/ModuleManager.h"

class IConsoleObject;

class FGuiyangMahjongEditorToolsModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void OpenRoomPresentationEditor();

    IConsoleObject* OpenRoomPresentationCommand = nullptr;
};
