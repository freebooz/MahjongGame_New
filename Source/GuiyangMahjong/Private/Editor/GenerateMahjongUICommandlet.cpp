#include "Editor/GenerateMahjongUICommandlet.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/CircularThrobber.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WrapBox.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "UObject/SavePackage.h"
#include "Engine/Texture2D.h"
#include "UI/MobileConnectServerWidget.h"
#include "UI/MobileLobbyWidget.h"
#include "UI/MobileRoomWidget.h"
#include "UI/MobileMahjongHUDWidget.h"
#include "UI/MobileHandTileWidget.h"
#include "UI/MobileDiscardTileWidget.h"
#include "UI/MobileActionButtonPanel.h"
#include "UI/MobileSettlementWidget.h"
#include "UI/MobileErrorToastWidget.h"
#include "UI/MobileReconnectOverlayWidget.h"
#include "UI/MobileLoginWidget.h"
#include "UI/MobileRootHUDWidget.h"
#include "UI/MobileConfirmDialogWidget.h"
#include "UI/MobileCreateRoomDialogWidget.h"
#include "UI/MobileJoinRoomDialogWidget.h"

namespace MahjongUIBuilder
{
    static const FLinearColor Gold(0.92f, 0.66f, 0.20f, 1.0f);
    static const FLinearColor DeepGreen(0.025f, 0.16f, 0.13f, 0.98f);
    static const FLinearColor PanelGreen(0.045f, 0.25f, 0.20f, 0.96f);
    static const FLinearColor WarmWhite(0.98f, 0.94f, 0.80f, 1.0f);

    static void MarkVariable(UWidgetBlueprint* BP, UWidget* Widget)
    {
        Widget->bIsVariable = true;
        BP->OnVariableAdded(Widget->GetFName());
    }

    static UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D Position, const FVector2D Size,
        const FAnchors Anchors = FAnchors(0.0f, 0.0f), const FVector2D Alignment = FVector2D::ZeroVector)
    {
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetAnchors(Anchors);
        Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        return Slot;
    }

    static UTextBlock* Text(UWidgetBlueprint* BP, const FName Name, const TCHAR* Value, const int32 Size = 28)
    {
        UTextBlock* Widget = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Widget->SetText(FText::FromString(Value));
        Widget->SetColorAndOpacity(FSlateColor(WarmWhite));
        FSlateFontInfo Font = Widget->GetFont(); Font.Size = Size; Widget->SetFont(Font);
        MarkVariable(BP, Widget);
        return Widget;
    }

    static FSlateBrush BoxBrush(const FLinearColor Color)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.Margin = FMargin(0.25f);
        Brush.TintColor = FSlateColor(Color);
        return Brush;
    }

    static FSlateBrush TextureBrush(const TCHAR* ObjectPath, const float Margin = 0.0f)
    {
        FSlateBrush Brush;
        if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, ObjectPath))
        {
            Brush.SetResourceObject(Texture);
            Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
        }
        Brush.DrawAs = Margin > 0.0f ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
        Brush.Margin = FMargin(Margin);
        return Brush;
    }

    static FString ButtonKind(const FName Name)
    {
        const FString Value = Name.ToString();
        if (Value.Contains(TEXT("Hu"))) return TEXT("Hu");
        if (Value.Contains(TEXT("Gang"))) return TEXT("Gang");
        if (Value.Contains(TEXT("Peng"))) return TEXT("Peng");
        if (Value.Contains(TEXT("Pass"))) return TEXT("Pass");
        if (Value.Contains(TEXT("PlayTile"))) return TEXT("PlayTile");
        if (Value.Contains(TEXT("Leave")) || Value.Contains(TEXT("Back")) || Value.Contains(TEXT("Cancel"))) return TEXT("NeutralDark");
        if (Value.Contains(TEXT("Connect")) || Value.Contains(TEXT("Quick")) || Value.Contains(TEXT("Ready")) || Value.Contains(TEXT("Confirm"))) return TEXT("PrimaryGold");
        return TEXT("PrimaryGreen");
    }

    static UButton* Button(UWidgetBlueprint* BP, const FName Name, const TCHAR* Label)
    {
        UButton* Widget = BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        FButtonStyle Style;
        const FString Kind = ButtonKind(Name);
        const FString Prefix = FString::Printf(TEXT("/Game/UI/Textures/Buttons/T_Btn_%s_"), *Kind);
        Style.SetNormal(TextureBrush(*FString::Printf(TEXT("%sNormal.T_Btn_%s_Normal"), *Prefix, *Kind), 0.1458f));
        Style.SetHovered(TextureBrush(*FString::Printf(TEXT("%sHovered.T_Btn_%s_Hovered"), *Prefix, *Kind), 0.1458f));
        Style.SetPressed(TextureBrush(*FString::Printf(TEXT("%sPressed.T_Btn_%s_Pressed"), *Prefix, *Kind), 0.1458f));
        Style.SetDisabled(TextureBrush(*FString::Printf(TEXT("%sDisabled.T_Btn_%s_Disabled"), *Prefix, *Kind), 0.1458f));
        Widget->SetStyle(Style);
        MarkVariable(BP, Widget);
        if (Label && Label[0] != TEXT('\0'))
        {
            UTextBlock* LabelWidget = Text(BP, *FString::Printf(TEXT("%s_Label"), *Name.ToString()), Label, 27);
            LabelWidget->SetJustification(ETextJustify::Center);
            Widget->AddChild(LabelWidget);
        }
        return Widget;
    }

    static void ReplaceButtonContent(UWidgetBlueprint* BP, UButton* Widget, UWidget* NewContent)
    {
        for (int32 ChildIndex = 0; ChildIndex < Widget->GetChildrenCount(); ++ChildIndex)
        {
            if (UWidget* OldContent = Widget->GetChildAt(ChildIndex))
            {
                BP->OnVariableRemoved(OldContent->GetFName());
            }
        }
        Widget->ClearChildren();
        Widget->AddChild(NewContent);
    }

    static UEditableTextBox* Edit(UWidgetBlueprint* BP, const FName Name, const TCHAR* Hint)
    {
        UEditableTextBox* Widget = BP->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
        Widget->SetHintText(FText::FromString(Hint));
        FEditableTextBoxStyle Style = Widget->WidgetStyle;
        Style.BackgroundImageNormal = TextureBrush(TEXT("/Game/UI/Textures/Controls/T_Input_Normal_9Slice.T_Input_Normal_9Slice"), 0.1094f);
        Style.BackgroundImageHovered = TextureBrush(TEXT("/Game/UI/Textures/Controls/T_Input_Focused_9Slice.T_Input_Focused_9Slice"), 0.1094f);
        Style.BackgroundImageFocused = TextureBrush(TEXT("/Game/UI/Textures/Controls/T_Input_Focused_9Slice.T_Input_Focused_9Slice"), 0.1094f);
        Style.BackgroundImageReadOnly = TextureBrush(TEXT("/Game/UI/Textures/Controls/T_Input_Disabled_9Slice.T_Input_Disabled_9Slice"), 0.1094f);
        Widget->WidgetStyle = Style;
        MarkVariable(BP, Widget);
        return Widget;
    }

    static UBorder* Border(UWidgetBlueprint* BP, const FName Name, const FLinearColor Color)
    {
        UBorder* Widget = BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        const FString Value = Name.ToString();
        const TCHAR* Asset = Value.Contains(TEXT("Toast")) ? TEXT("/Game/UI/Textures/Panels/T_Panel_Toast_9Slice.T_Panel_Toast_9Slice")
            : Value.Contains(TEXT("Dialog")) ? TEXT("/Game/UI/Textures/Panels/T_Panel_Dialog_DarkGreen_9Slice.T_Panel_Dialog_DarkGreen_9Slice")
            : Value.Contains(TEXT("Reconnect")) ? TEXT("/Game/UI/Textures/Panels/T_Panel_Notice_9Slice.T_Panel_Notice_9Slice")
            : TEXT("/Game/UI/Textures/Panels/T_Panel_Main_GreenGold_9Slice.T_Panel_Main_GreenGold_9Slice");
        FSlateBrush Brush = TextureBrush(Asset, 0.1563f);
        Brush.TintColor = FSlateColor(Color);
        Widget->SetBrush(Brush);
        Widget->SetPadding(FMargin(24.0f));
        MarkVariable(BP, Widget);
        return Widget;
    }

    static UImage* Image(UWidgetBlueprint* BP, const FName Name, const FLinearColor Color)
    {
        UImage* Widget = BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
        if (Name == TEXT("Img_Background"))
        {
            Widget->SetBrush(TextureBrush(TEXT("/Game/UI/Textures/Backgrounds/T_BG_Login_Guiyang.T_BG_Login_Guiyang")));
        }
        else
        {
            Widget->SetColorAndOpacity(Color);
        }
        MarkVariable(BP, Widget);
        return Widget;
    }

    static void AddBackground(UWidgetBlueprint* BP, UCanvasPanel* Canvas, const TCHAR* ObjectPath)
    {
        UImage* Background = BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Img_VisualBackground"));
        Background->SetBrush(TextureBrush(ObjectPath));
        MarkVariable(BP, Background);
        UCanvasPanelSlot* Slot = Place(Canvas, Background, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0, 0, 1, 1));
        Slot->SetOffsets(FMargin(0));
        Slot->SetZOrder(-10);
    }

    static UCanvasPanel* Root(UWidgetBlueprint* BP)
    {
        BP->Modify();
        if (BP->WidgetTree)
        {
            // 整棵树重建时所有旧变量都会失效，直接清空 GUID 映射可避免已移除的嵌套控件残留。
            BP->WidgetVariableNameToGuidMap.Reset();
            BP->WidgetTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
        }
        BP->WidgetTree = NewObject<UWidgetTree>(BP, TEXT("WidgetTree"), RF_Transactional | RF_ArchetypeObject);
        USafeZone* Safe = BP->WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("SafeZone_Root"));
        UScaleBox* Scale = BP->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("Scale_Design1920x1080"));
        UCanvasPanel* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Root"));
        Safe->AddChild(Scale); Scale->AddChild(Canvas);
        Scale->SetStretch(EStretch::ScaleToFit);
        Scale->SetStretchDirection(EStretchDirection::Both);
        BP->WidgetTree->RootWidget = Safe;
        MarkVariable(BP, Safe); MarkVariable(BP, Scale); MarkVariable(BP, Canvas);
        const FString BPName = BP->GetName();
        const TCHAR* BackgroundPath = BPName == TEXT("WBP_Lobby") ? TEXT("/Game/UI/Textures/Backgrounds/T_BG_Lobby_JiaxiuTower.T_BG_Lobby_JiaxiuTower")
            : BPName == TEXT("WBP_Room") ? TEXT("/Game/UI/Textures/Backgrounds/T_BG_Room_GuiyangNight.T_BG_Room_GuiyangNight")
            : BPName == TEXT("WBP_GameHUD") ? TEXT("/Game/UI/Textures/Backgrounds/T_BG_GameTable_GreenFelt.T_BG_GameTable_GreenFelt")
            : BPName == TEXT("WBP_Settlement") ? TEXT("/Game/UI/Textures/Backgrounds/T_BG_Settlement_GuiyangRiver.T_BG_Settlement_GuiyangRiver")
            : BPName == TEXT("WBP_ConnectServer") ? TEXT("/Game/UI/Textures/Backgrounds/T_BG_Login_Guiyang.T_BG_Login_Guiyang")
            : nullptr;
        UWidget* Background = nullptr;
        if (BackgroundPath)
        {
            UImage* BackgroundImage = BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background_ComponentSlot"));
            BackgroundImage->SetBrush(TextureBrush(BackgroundPath));
            MarkVariable(BP, BackgroundImage);
            Background = BackgroundImage;
        }
        else
        {
            Background = Border(BP, TEXT("Background_ComponentSlot"), DeepGreen);
        }
        UCanvasPanelSlot* Slot = Place(Canvas, Background, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0,0,1,1));
        Slot->SetOffsets(FMargin(0));
        return Canvas;
    }

    static UWidgetBlueprint* Create(const TCHAR* Folder, const TCHAR* Name, UClass* ParentClass)
    {
        const FString PackageName = FString::Printf(TEXT("/Game/UI/%s/%s"), Folder, Name);
        const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, Name);
        if (UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath))
        {
            Existing->ParentClass = ParentClass;
            return Existing;
        }
        UPackage* Package = CreatePackage(*PackageName);
        UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
        Factory->ParentClass = ParentClass;
        UWidgetBlueprint* BP = Cast<UWidgetBlueprint>(Factory->FactoryCreateNew(UWidgetBlueprint::StaticClass(), Package, FName(Name), RF_Public | RF_Standalone, nullptr, GWarn));
        if (BP) FAssetRegistryModule::AssetCreated(BP);
        return BP;
    }

    static bool Save(UWidgetBlueprint* BP)
    {
        const TArray<UWidget*> SourceWidgets = BP->GetAllSourceWidgets();
        UE_LOG(LogTemp, Display, TEXT("编译前控件树检查：%s，控件数=%d"), *BP->GetPathName(), SourceWidgets.Num());

        // 在编译前验证 C++ BindWidget 契约，避免只生成“看得见但不可用”的蓝图。
        bool bBindingsValid = true;
        for (TFieldIterator<FObjectPropertyBase> PropertyIt(BP->ParentClass, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
        {
            const FObjectPropertyBase* Property = *PropertyIt;
            if (!Property->HasMetaData(TEXT("BindWidget")))
            {
                continue;
            }

            UWidget* BoundWidget = BP->WidgetTree->FindWidget(Property->GetFName());
            UClass* RequiredClass = Property->PropertyClass;
            if (!BoundWidget || !BoundWidget->IsA(RequiredClass))
            {
                UE_LOG(LogTemp, Error, TEXT("控件绑定验证失败：%s.%s，期望类型=%s，实际=%s"),
                    *BP->GetName(), *Property->GetName(), *GetNameSafe(RequiredClass), *GetNameSafe(BoundWidget));
                bBindingsValid = false;
            }
        }
        if (!bBindingsValid)
        {
            return false;
        }

        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipGarbageCollection);
        if (BP->Status == BS_Error) return false;
        UPackage* Package = BP->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, BP, *Filename, Args);
    }

    static UHorizontalBox* Horizontal(UWidgetBlueprint* BP, const FName Name)
    {
        UHorizontalBox* W = BP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), Name); MarkVariable(BP, W); return W;
    }
    static UVerticalBox* Vertical(UWidgetBlueprint* BP, const FName Name)
    {
        UVerticalBox* W = BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name); MarkVariable(BP, W); return W;
    }
    static UWrapBox* Wrap(UWidgetBlueprint* BP, const FName Name)
    {
        UWrapBox* W = BP->WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), Name); MarkVariable(BP, W); return W;
    }
}

UGenerateMahjongUICommandlet::UGenerateMahjongUICommandlet()
{
    IsClient = false; IsEditor = true; IsServer = false; LogToConsole = true;
}

int32 UGenerateMahjongUICommandlet::Main(const FString& Params)
{
    using namespace MahjongUIBuilder;
    int32 Created = 0;
    auto Finish = [&Created](UWidgetBlueprint* BP)
    {
        if (BP && Save(BP)) { ++Created; UE_LOG(LogTemp, Display, TEXT("UMG资产已生成：%s"), *BP->GetPathName()); return true; }
        UE_LOG(LogTemp, Error, TEXT("UMG资产生成失败")); return false;
    };

    // 手牌和弃牌组件先生成，供主 HUD 动态创建。
    UWidgetBlueprint* Hand = Create(TEXT("Components"), TEXT("WBP_HandTile"), UMobileHandTileWidget::StaticClass());
    { UCanvasPanel* C = Root(Hand); UButton* B = Button(Hand, TEXT("Btn_Tile"), TEXT("")); Place(C, B, {0,0}, {92,128}); UTextBlock* T = Text(Hand, TEXT("Txt_TileName"), TEXT("一万"), 30); ReplaceButtonContent(Hand, B, T); }
    Finish(Hand);

    UWidgetBlueprint* Discard = Create(TEXT("Components"), TEXT("WBP_DiscardTile"), UMobileDiscardTileWidget::StaticClass());
    { UCanvasPanel* C = Root(Discard); UBorder* B = Border(Discard, TEXT("Border_Tile"), WarmWhite); Place(C, B, {0,0}, {64,88}); UTextBlock* T = Text(Discard, TEXT("Txt_TileName"), TEXT("一万"), 22); T->SetColorAndOpacity(FSlateColor(FLinearColor::Black)); B->AddChild(T); }
    Finish(Discard);

    UWidgetBlueprint* Action = Create(TEXT("Components"), TEXT("WBP_ActionButtonPanel"), UMobileActionButtonPanel::StaticClass());
    { UCanvasPanel* C = Root(Action); UHorizontalBox* H = Horizontal(Action, TEXT("Panel_Actions")); Place(C,H,{0,0},{600,96}); H->AddChildToHorizontalBox(Button(Action,TEXT("Btn_Hu"),TEXT("胡"))); H->AddChildToHorizontalBox(Button(Action,TEXT("Btn_Gang"),TEXT("杠"))); H->AddChildToHorizontalBox(Button(Action,TEXT("Btn_Peng"),TEXT("碰"))); H->AddChildToHorizontalBox(Button(Action,TEXT("Btn_Pass"),TEXT("过"))); }
    Finish(Action);

    UWidgetBlueprint* Toast = Create(TEXT("Components"), TEXT("WBP_ErrorToast"), UMobileErrorToastWidget::StaticClass());
    { UCanvasPanel* C=Root(Toast); UBorder* B=Border(Toast,TEXT("Border_Toast"),FLinearColor(0.55f,0.08f,0.06f,0.96f)); Place(C,B,{0,70},{800,84},FAnchors(0.5f,0),FVector2D(0.5f,0)); B->AddChild(Text(Toast,TEXT("Txt_Message"),TEXT("操作失败，请重试"),28)); }
    Finish(Toast);

    UWidgetBlueprint* Login = Create(TEXT("Screens"), TEXT("WBP_Login"), UMobileLoginWidget::StaticClass());
    { UCanvasPanel* C=Root(Login); UImage* Bg=Image(Login,TEXT("Img_Background"),FLinearColor(0.02f,0.12f,0.10f,1.0f)); UCanvasPanelSlot* BgSlot=Place(C,Bg,{0,0},{0,0},FAnchors(0,0,1,1)); BgSlot->SetOffsets(FMargin(0)); UImage* Logo=Image(Login,TEXT("Img_GameLogo"),Gold); Place(C,Logo,{0,100},{420,150},FAnchors(0.5f,0),{0.5f,0}); Place(C,Text(Login,TEXT("Txt_LoginStatus"),TEXT("请选择登录方式"),28),{0,300},{700,52},FAnchors(0.5f,0),{0.5f,0}); UCircularThrobber* Loading=Login->WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass(),TEXT("Loading_Login")); MarkVariable(Login,Loading); Loading->SetVisibility(ESlateVisibility::Collapsed); Place(C,Loading,{0,370},{64,64},FAnchors(0.5f,0),{0.5f,0}); Place(C,Button(Login,TEXT("Btn_WechatLogin"),TEXT("微信登录（PC 模拟授权）")),{-230,470},{460,78},FAnchors(0.5f,0)); Place(C,Button(Login,TEXT("Btn_GuestLogin"),TEXT("游客登录")),{-230,570},{460,78},FAnchors(0.5f,0)); UCheckBox* Terms=Login->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("Chk_AgreeTerms")); MarkVariable(Login,Terms); Place(C,Terms,{-300,680},{44,44},FAnchors(0.5f,0)); Place(C,Text(Login,TEXT("Txt_AgreeHint"),TEXT("我已阅读并同意"),24),{-245,682},{180,40},FAnchors(0.5f,0)); Place(C,Button(Login,TEXT("Btn_UserAgreement"),TEXT("用户协议")),{-60,672},{170,56},FAnchors(0.5f,0)); Place(C,Button(Login,TEXT("Btn_PrivacyPolicy"),TEXT("隐私政策")),{125,672},{170,56},FAnchors(0.5f,0)); Place(C,Text(Login,TEXT("Txt_Version"),TEXT("版本 0.2.0 · UE 5.8"),22),{30,-54},{360,36},FAnchors(0,1)); }
    Finish(Login);

    UWidgetBlueprint* Confirm = Create(TEXT("Dialogs"), TEXT("WBP_ConfirmDialog"), UMobileConfirmDialogWidget::StaticClass());
    { UCanvasPanel* C=Root(Confirm); UBorder* Mask=Border(Confirm,TEXT("Border_Mask"),FLinearColor(0,0,0,0.65f)); UCanvasPanelSlot* MaskSlot=Place(C,Mask,{0,0},{0,0},FAnchors(0,0,1,1)); MaskSlot->SetOffsets(FMargin(0)); UBorder* Dialog=Border(Confirm,TEXT("Border_Dialog9Slice"),PanelGreen); Place(C,Dialog,{0,0},{760,420},FAnchors(0.5f,0.5f),{0.5f,0.5f}); UVerticalBox* V=Vertical(Confirm,TEXT("Panel_ConfirmContent")); Dialog->AddChild(V); V->AddChildToVerticalBox(Text(Confirm,TEXT("Txt_Title"),TEXT("请确认"),42)); V->AddChildToVerticalBox(Text(Confirm,TEXT("Txt_Message"),TEXT("是否继续当前操作？"),28)); UHorizontalBox* Buttons=Horizontal(Confirm,TEXT("Panel_ConfirmButtons")); Buttons->AddChildToHorizontalBox(Button(Confirm,TEXT("Btn_Confirm"),TEXT("确认"))); Buttons->AddChildToHorizontalBox(Button(Confirm,TEXT("Btn_Cancel"),TEXT("取消"))); V->AddChildToVerticalBox(Buttons); }
    Finish(Confirm);

    UWidgetBlueprint* CreateRoom = Create(TEXT("Dialogs"), TEXT("WBP_CreateRoomDialog"), UMobileCreateRoomDialogWidget::StaticClass());
    { UCanvasPanel* C=Root(CreateRoom); UBorder* Mask=Border(CreateRoom,TEXT("Border_Mask"),FLinearColor(0,0,0,0.65f)); UCanvasPanelSlot* MaskSlot=Place(C,Mask,{0,0},{0,0},FAnchors(0,0,1,1)); MaskSlot->SetOffsets(FMargin(0)); UBorder* Dialog=Border(CreateRoom,TEXT("Border_Dialog9Slice"),PanelGreen); Place(C,Dialog,{0,0},{840,650},FAnchors(0.5f,0.5f),{0.5f,0.5f}); UVerticalBox* V=Vertical(CreateRoom,TEXT("Panel_CreateRoomContent")); Dialog->AddChild(V); V->AddChildToVerticalBox(Text(CreateRoom,TEXT("Txt_Title"),TEXT("创建房间"),42)); V->AddChildToVerticalBox(Text(CreateRoom,TEXT("Txt_RoundCountLabel"),TEXT("局数（1-16）"),24)); V->AddChildToVerticalBox(Edit(CreateRoom,TEXT("Txt_RoundCount"),TEXT("默认 4 局"))); UHorizontalBox* PublicRow=Horizontal(CreateRoom,TEXT("Panel_PublicRoom")); UCheckBox* Public=CreateRoom->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("Chk_PublicRoom")); MarkVariable(CreateRoom,Public); PublicRow->AddChildToHorizontalBox(Public); PublicRow->AddChildToHorizontalBox(Text(CreateRoom,TEXT("Txt_PublicRoomLabel"),TEXT("允许快速匹配加入"),24)); V->AddChildToVerticalBox(PublicRow); UHorizontalBox* PasswordRow=Horizontal(CreateRoom,TEXT("Panel_EnablePassword")); UCheckBox* EnablePassword=CreateRoom->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("Chk_EnablePassword")); MarkVariable(CreateRoom,EnablePassword); PasswordRow->AddChildToHorizontalBox(EnablePassword); PasswordRow->AddChildToHorizontalBox(Text(CreateRoom,TEXT("Txt_EnablePasswordLabel"),TEXT("启用房间密码"),24)); V->AddChildToVerticalBox(PasswordRow); UEditableTextBox* Password=Edit(CreateRoom,TEXT("Txt_Password"),TEXT("6-12 位可选密码")); Password->SetIsPassword(true); V->AddChildToVerticalBox(Password); V->AddChildToVerticalBox(Text(CreateRoom,TEXT("Txt_Status"),TEXT("使用默认贵阳主流规则创建房间"),22)); UHorizontalBox* Buttons=Horizontal(CreateRoom,TEXT("Panel_CreateRoomButtons")); Buttons->AddChildToHorizontalBox(Button(CreateRoom,TEXT("Btn_Create"),TEXT("创建"))); Buttons->AddChildToHorizontalBox(Button(CreateRoom,TEXT("Btn_Cancel"),TEXT("取消"))); V->AddChildToVerticalBox(Buttons); }
    Finish(CreateRoom);

    UWidgetBlueprint* JoinRoom = Create(TEXT("Dialogs"), TEXT("WBP_JoinRoomDialog"), UMobileJoinRoomDialogWidget::StaticClass());
    { UCanvasPanel* C=Root(JoinRoom); UBorder* Mask=Border(JoinRoom,TEXT("Border_Mask"),FLinearColor(0,0,0,0.65f)); UCanvasPanelSlot* MaskSlot=Place(C,Mask,{0,0},{0,0},FAnchors(0,0,1,1)); MaskSlot->SetOffsets(FMargin(0)); UBorder* Dialog=Border(JoinRoom,TEXT("Border_Dialog9Slice"),PanelGreen); Place(C,Dialog,{0,0},{760,520},FAnchors(0.5f,0.5f),{0.5f,0.5f}); UVerticalBox* V=Vertical(JoinRoom,TEXT("Panel_JoinRoomContent")); Dialog->AddChild(V); V->AddChildToVerticalBox(Text(JoinRoom,TEXT("Txt_Title"),TEXT("加入房间"),42)); V->AddChildToVerticalBox(Edit(JoinRoom,TEXT("Txt_RoomCode"),TEXT("请输入 6 位房间号"))); UEditableTextBox* Password=Edit(JoinRoom,TEXT("Txt_Password"),TEXT("密码房请输入密码")); Password->SetIsPassword(true); V->AddChildToVerticalBox(Password); V->AddChildToVerticalBox(Text(JoinRoom,TEXT("Txt_Status"),TEXT("请输入 6 位房间号"),22)); UHorizontalBox* Buttons=Horizontal(JoinRoom,TEXT("Panel_JoinRoomButtons")); Buttons->AddChildToHorizontalBox(Button(JoinRoom,TEXT("Btn_Join"),TEXT("加入"))); Buttons->AddChildToHorizontalBox(Button(JoinRoom,TEXT("Btn_Cancel"),TEXT("取消"))); V->AddChildToVerticalBox(Buttons); }
    Finish(JoinRoom);

    UWidgetBlueprint* RootHUD = Create(TEXT("Screens"), TEXT("WBP_RootHUD"), UMobileRootHUDWidget::StaticClass());
    { UCanvasPanel* C=Root(RootHUD); UOverlay* Screen=RootHUD->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("ScreenLayer")); MarkVariable(RootHUD,Screen); UCanvasPanelSlot* ScreenSlot=Place(C,Screen,{0,0},{0,0},FAnchors(0,0,1,1)); ScreenSlot->SetOffsets(FMargin(0)); UOverlay* Popup=RootHUD->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("PopupLayer")); MarkVariable(RootHUD,Popup); UCanvasPanelSlot* PopupSlot=Place(C,Popup,{0,0},{0,0},FAnchors(0,0,1,1)); PopupSlot->SetOffsets(FMargin(0)); PopupSlot->SetZOrder(100); }
    Finish(RootHUD);

    UWidgetBlueprint* Connect = Create(TEXT("Screens"), TEXT("WBP_ConnectServer"), UMobileConnectServerWidget::StaticClass());
    { UCanvasPanel* C=Root(Connect); Place(C,Text(Connect,TEXT("Title"),TEXT("贵阳捉鸡麻将"),54),{0,120},{700,80},FAnchors(0.5f,0),{0.5f,0}); Place(C,Edit(Connect,TEXT("Txt_ServerIP"),TEXT("服务器 IP，例如 127.0.0.1")),{-230,300},{460,64},FAnchors(0.5f,0)); Place(C,Edit(Connect,TEXT("Txt_ServerPort"),TEXT("端口 7777")),{-230,380},{460,64},FAnchors(0.5f,0)); Place(C,Edit(Connect,TEXT("Txt_PlayerName"),TEXT("请输入中文昵称")),{-230,460},{460,64},FAnchors(0.5f,0)); UCheckBox* Chk=Connect->WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),TEXT("Chk_RememberAddress")); MarkVariable(Connect,Chk); Place(C,Chk,{-230,540},{44,44},FAnchors(0.5f,0)); UButton* Btn=Button(Connect,TEXT("Btn_Connect"),TEXT("")); Place(C,Btn,{-230,620},{460,76},FAnchors(0.5f,0)); ReplaceButtonContent(Connect, Btn, Text(Connect,TEXT("Txt_ConnectButton"),TEXT("连接服务器"),30)); Place(C,Text(Connect,TEXT("Txt_Version"),TEXT("版本 0.1.0 · UE 5.8"),20),{24,-52},{340,36},FAnchors(0,1)); }
    Finish(Connect);

    UWidgetBlueprint* Lobby = Create(TEXT("Screens"), TEXT("WBP_Lobby"), UMobileLobbyWidget::StaticClass());
    { UCanvasPanel* C=Root(Lobby); Place(C,Text(Lobby,TEXT("Txt_PlayerName"),TEXT("玩家昵称"),34),{80,70},{420,54}); Place(C,Text(Lobby,TEXT("Txt_PlayerId"),TEXT("玩家ID：--"),22),{80,130},{420,40}); Place(C,Text(Lobby,TEXT("Txt_OnlineCount"),TEXT("在线人数：0"),22),{80,180},{420,40}); Place(C,Button(Lobby,TEXT("Btn_QuickStart"),TEXT("快速开始")),{710,320},{500,110}); Place(C,Button(Lobby,TEXT("Btn_CreateRoom"),TEXT("创建房间")),{710,460},{240,82}); Place(C,Button(Lobby,TEXT("Btn_JoinRoom"),TEXT("加入房间")),{970,460},{240,82}); Place(C,Button(Lobby,TEXT("Btn_Setting"),TEXT("设置")),{1660,60},{180,64}); }
    Finish(Lobby);

    UWidgetBlueprint* Room = Create(TEXT("Screens"), TEXT("WBP_Room"), UMobileRoomWidget::StaticClass());
    { UCanvasPanel* C=Root(Room); Place(C,Text(Room,TEXT("Txt_RoomId"),TEXT("房间号：100001"),34),{60,50},{500,60}); Place(C,Text(Room,TEXT("Txt_RuleSummary"),TEXT("贵阳捉鸡·四人房"),24),{60,115},{500,44}); Place(C,Text(Room,TEXT("Seat_Top"),TEXT("等待玩家"),25),{760,120},{400,100}); Place(C,Text(Room,TEXT("Seat_Left"),TEXT("等待玩家"),25),{140,430},{400,100}); Place(C,Text(Room,TEXT("Seat_Right"),TEXT("等待玩家"),25),{1380,430},{400,100}); Place(C,Text(Room,TEXT("Seat_Bottom"),TEXT("等待玩家"),25),{760,720},{400,100}); Place(C,Button(Room,TEXT("Btn_Ready"),TEXT("准备")),{760,860},{240,80}); Place(C,Button(Room,TEXT("Btn_LeaveRoom"),TEXT("离开房间")),{1020,860},{240,80}); Place(C,Text(Room,TEXT("Txt_StartTip"),TEXT("满四人并准备后自动开始"),24),{710,970},{520,48}); }
    Finish(Room);

    UWidgetBlueprint* Settlement = Create(TEXT("Dialogs"), TEXT("WBP_Settlement"), UMobileSettlementWidget::StaticClass());
    { UCanvasPanel* C=Root(Settlement); UBorder* B=Border(Settlement,TEXT("Panel_Dialog9Slice"),PanelGreen); Place(C,B,{0,0},{980,720},FAnchors(0.5f,0.5f),{0.5f,0.5f}); UVerticalBox* V=Vertical(Settlement,TEXT("Panel_SettlementContent")); B->AddChild(V); V->AddChildToVerticalBox(Text(Settlement,TEXT("Txt_ResultTitle"),TEXT("本局结算"),42)); V->AddChildToVerticalBox(Text(Settlement,TEXT("Txt_HuType"),TEXT("自摸"),28)); V->AddChildToVerticalBox(Text(Settlement,TEXT("Txt_JiResult"),TEXT("鸡牌：0 张"),28)); V->AddChildToVerticalBox(Vertical(Settlement,TEXT("Panel_PlayerScores"))); V->AddChildToVerticalBox(Button(Settlement,TEXT("Btn_NextRound"),TEXT("再来一局"))); V->AddChildToVerticalBox(Button(Settlement,TEXT("Btn_BackLobby"),TEXT("返回大厅"))); }
    Finish(Settlement);

    UWidgetBlueprint* Reconnect = Create(TEXT("Dialogs"), TEXT("WBP_ReconnectOverlay"), UMobileReconnectOverlayWidget::StaticClass());
    { UCanvasPanel* C=Root(Reconnect); UBorder* B=Border(Reconnect,TEXT("Panel_Reconnect9Slice"),FLinearColor(0.01f,0.05f,0.045f,0.94f)); Place(C,B,{0,0},{760,460},FAnchors(0.5f,0.5f),{0.5f,0.5f}); UVerticalBox* V=Vertical(Reconnect,TEXT("Panel_ReconnectContent")); B->AddChild(V); V->AddChildToVerticalBox(Text(Reconnect,TEXT("Txt_ReconnectStatus"),TEXT("网络连接已断开，正在尝试重连"),30)); V->AddChildToVerticalBox(Text(Reconnect,TEXT("Txt_RemainingTime"),TEXT("剩余 120 秒"),25)); V->AddChildToVerticalBox(Button(Reconnect,TEXT("Btn_Reconnect"),TEXT("重新连接"))); V->AddChildToVerticalBox(Button(Reconnect,TEXT("Btn_BackConnect"),TEXT("返回连接界面"))); }
    Finish(Reconnect);

    UWidgetBlueprint* HUD = Create(TEXT("Screens"), TEXT("WBP_GameHUD"), UMobileMahjongHUDWidget::StaticClass());
    { UCanvasPanel* C=Root(HUD); Place(C,Text(HUD,TEXT("Txt_RoomId"),TEXT("房间：100001"),22),{30,24},{320,40}); Place(C,Text(HUD,TEXT("Txt_RemainingTileCount"),TEXT("剩余：83"),28),{800,30},{320,45}); Place(C,Text(HUD,TEXT("Txt_CurrentPhase"),TEXT("阶段：玩家回合"),22),{30,70},{360,40}); Place(C,Text(HUD,TEXT("Txt_CurrentTurnPlayer"),TEXT("当前座位：0"),22),{800,80},{320,40}); Place(C,Text(HUD,TEXT("Txt_Countdown"),TEXT("15"),38),{1500,40},{120,60}); Place(C,Wrap(HUD,TEXT("Panel_SelfDiscards")),{620,620},{680,180}); Place(C,Wrap(HUD,TEXT("Panel_TopDiscards")),{620,230},{680,180}); Place(C,Wrap(HUD,TEXT("Panel_LeftDiscards")),{240,330},{300,380}); Place(C,Wrap(HUD,TEXT("Panel_RightDiscards")),{1380,330},{300,380}); Place(C,Text(HUD,TEXT("Seat_Self"),TEXT("我\n13张\n0分"),22),{80,820},{260,100}); Place(C,Text(HUD,TEXT("Seat_Top"),TEXT("玩家\n13张\n0分"),22),{830,130},{260,100}); Place(C,Text(HUD,TEXT("Seat_Left"),TEXT("玩家\n13张\n0分"),22),{70,400},{260,100}); Place(C,Text(HUD,TEXT("Seat_Right"),TEXT("玩家\n13张\n0分"),22),{1600,400},{260,100}); Place(C,Horizontal(HUD,TEXT("Panel_SelfHandTiles")),{260,890},{1400,150}); UClass* ActionClass=Action->GeneratedClass; UWidget* ActionWidget=HUD->WidgetTree->ConstructWidget<UWidget>(ActionClass,TEXT("ActionButtonPanel")); MarkVariable(HUD,ActionWidget); Place(C,ActionWidget,{660,780},{600,96}); UOverlay* Popup=HUD->WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("PopupLayer")); MarkVariable(HUD,Popup); UCanvasPanelSlot* PopupSlot=Place(C,Popup,{0,0},{0,0},FAnchors(0,0,1,1)); PopupSlot->SetOffsets(FMargin(0)); }
    Finish(HUD);

    UE_LOG(LogTemp, Display, TEXT("P0 UMG 生成结束：成功=%d/15"), Created);
    return Created == 15 ? 0 : 1;
}
#else
UGenerateMahjongUICommandlet::UGenerateMahjongUICommandlet()
{
    IsClient = false; IsEditor = false; IsServer = false; LogToConsole = true;
}

int32 UGenerateMahjongUICommandlet::Main(const FString& Params)
{
    UE_LOG(LogTemp, Error, TEXT("UMG 资产生成命令只能在 Editor Target 中运行"));
    return 1;
}
#endif
