#include "GuiyangMahjongEditorTools.h"

#include "BlueprintEditorModule.h"
#include "BlueprintEditorTabs.h"
#include "ContentBrowserModule.h"
#include "Engine/Blueprint.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

namespace
{
constexpr TCHAR RoomPresentationAssetPath[] =
    TEXT("/Game/Client/Room/Presentation/"
         "BP_MahjongRoomPresentation.BP_MahjongRoomPresentation");
}

void FGuiyangMahjongEditorToolsModule::StartupModule()
{
    OpenRoomPresentationCommand = IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("Mahjong.OpenRoomPresentationEditor"),
        TEXT("Open the Mahjong room presentation Blueprint in the full Components/Viewport editor."),
        FConsoleCommandDelegate::CreateRaw(
            this, &FGuiyangMahjongEditorToolsModule::OpenRoomPresentationEditor),
        ECVF_Default);
}

void FGuiyangMahjongEditorToolsModule::ShutdownModule()
{
    if (OpenRoomPresentationCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(OpenRoomPresentationCommand);
        OpenRoomPresentationCommand = nullptr;
    }
}

void FGuiyangMahjongEditorToolsModule::OpenRoomPresentationEditor()
{
    if (IConsoleVariable* PrimingLimit =
            IConsoleManager::Get().FindConsoleVariable(TEXT("bp.DatabasePrimingMaxPerFrame")))
    {
        // UE 5.8 can crash while priming an AnimGraph node template even when
        // the opened asset is an Actor Blueprint. Manual room composition
        // does not need this optional background cache.
        PrimingLimit->Set(0, ECVF_SetByCode);
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, RoomPresentationAssetPath);
    if (!Blueprint)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("Mahjong room presentation Blueprint was not found: %s"),
            RoomPresentationAssetPath);
        return;
    }

    FContentBrowserModule& ContentBrowser =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    ContentBrowser.Get().SyncBrowserToAssets({FAssetData(Blueprint)});

    FBlueprintEditorModule& BlueprintEditorModule =
        FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
    const TSharedRef<IBlueprintEditor> BlueprintEditor =
        BlueprintEditorModule.CreateBlueprintEditor(
            EToolkitMode::Standalone,
            TSharedPtr<IToolkitHost>(),
            Blueprint,
            false);
    BlueprintEditor->GetTabManager()->TryInvokeTab(
        FBlueprintEditorTabs::SCSViewportID);

    UE_LOG(
        LogTemp,
        Display,
        TEXT("MAHJONG_FULL_BLUEPRINT_EDITOR_OPEN_OK asset=%s mode=%s"),
        RoomPresentationAssetPath,
        *BlueprintEditor->GetCurrentMode().ToString());
}

IMPLEMENT_MODULE(FGuiyangMahjongEditorToolsModule, GuiyangMahjongEditorTools)
