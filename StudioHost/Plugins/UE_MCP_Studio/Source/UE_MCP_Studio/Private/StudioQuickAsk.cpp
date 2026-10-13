// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioQuickAsk.h"

#include "StudioDaemon.h"
#include "StudioDock.h"
#include "AssetRegistry/AssetData.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SceneView.h"
#include "SLevelViewport.h"
#include "Widgets/SViewport.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "UE_MCP_Studio_QuickAsk"

DEFINE_LOG_CATEGORY_STATIC(LogStudioQuickAsk, Log, All);

FStudioQuickAskCommands::FStudioQuickAskCommands()
	: TCommands<FStudioQuickAskCommands>(TEXT("UEMCPStudio"), LOCTEXT("CommandsContext", "UE-MCP Studio"), NAME_None, FAppStyle::GetAppStyleSetName())
{
}

void FStudioQuickAskCommands::RegisterCommands()
{
	// Ctrl+Shift+Space is the viewport's Next view option; Ctrl+Alt+Space is unbound in UE 5.7 and 5.8.
	UI_COMMAND(AskStudio, "Ask Studio", "Open a one-line prompt at the cursor that starts a new UE-MCP Studio session", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::SpaceBar));
}

namespace StudioQuickAsk
{
	constexpr int32 MaxChips = 5;
	/** A deactivation this soon after opening is the click or menu that opened the pill letting go of focus. */
	constexpr double SettleSeconds = 0.3;

	class SPill;
	TSharedPtr<SWindow> PillWindow;
	TWeakPtr<SPill> Pill;
	double OpenedAt = 0.0;
	TSharedPtr<class FInput> Input;
	FDelegateHandle ActorMenuHandle;
	TArray<IConsoleObject*> ConsoleCommands;

	/** The pill's corner radius, in Slate units. */
	constexpr float PillRadius = 18.0f;

	const FSlateBrush* PillBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Panel, PillRadius, FStyleColors::InputOutline, 1.0f);
		return &Brush;
	}

	const FSlateBrush* ChipBrush()
	{
		static const FSlateRoundedBoxBrush Brush(FStyleColors::Input, 9.0f, FStyleColors::InputOutline, 1.0f);
		return &Brush;
	}

	FText ChipText(const FStudioQuickContext& C)
	{
		if (C.Kind == TEXT("location"))
		{
			return C.Label.IsEmpty() ? LOCTEXT("HereChip", "Here") : FText::Format(LOCTEXT("HereOnChip", "Here, on {0}"), FText::FromString(C.Label));
		}
		return C.Class.IsEmpty() || C.Kind == TEXT("asset")
			? FText::FromString(C.Label)
			: FText::Format(LOCTEXT("ActorChip", "{0} ({1})"), FText::FromString(C.Label), FText::FromString(C.Class));
	}

	/** Every key the pill does not use stops here, so typing never reaches an editor shortcut. Esc closes it. */
	class SPill : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SPill) {}
			SLATE_ARGUMENT(TArray<FStudioQuickContext>, Context)
			SLATE_ARGUMENT(FString, Text)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Context = InArgs._Context;
			TSharedRef<SWrapBox> Chips = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(4.0, 4.0));
			for (int32 i = 0; i < Context.Num() && i < MaxChips; ++i)
			{
				Chips->AddSlot()[Chip(ChipText(Context[i]), FText::FromString(Context[i].Path))];
			}
			if (Context.Num() > MaxChips)
			{
				Chips->AddSlot()[Chip(FText::Format(LOCTEXT("MoreChips", "+{0}"), Context.Num() - MaxChips), FText::GetEmpty())];
			}

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(PillBrush())
				.Padding(FMargin(14.0f, 8.0f, 8.0f, 6.0f))
				[
					SNew(SBox)
					.WidthOverride(440.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
						[
							SNew(SBox).Visibility(Context.Num() ? EVisibility::Visible : EVisibility::Collapsed)[Chips]
						]
						+ SVerticalBox::Slot().AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
							[
								SAssignNew(Box, SEditableTextBox)
								.Text(FText::FromString(InArgs._Text))
								.HintText(Context.Num() ? LOCTEXT("HintContext", "Ask about this") : LOCTEXT("Hint", "Ask Studio"))
								.IsEnabled_Lambda([this] { return !bSending; })
								.OnTextCommitted(this, &SPill::OnCommitted)
								.OnKeyDownHandler(this, &SPill::OnBoxKey)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
								.Text_Lambda([this] { return bSending ? LOCTEXT("Sending", "Sending") : LOCTEXT("Send", "Send"); })
								.IsEnabled_Lambda([this] { return !bSending; })
								.OnClicked_Lambda([this] { Send(); return FReply::Handled(); })
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Font(FAppStyle::GetFontStyle("SmallFont"))
							.AutoWrapText(true)
							.ColorAndOpacity_Lambda([this] { return Error.IsEmpty() ? FSlateColor::UseSubduedForeground() : FSlateColor(FStyleColors::Error); })
							.Text_Lambda([this] { return Error.IsEmpty() ? LOCTEXT("KeysHint", "Enter to send, Esc to close") : Error; })
						]
					]
				]
			];
		}

		void FocusBox()
		{
			FSlateApplication::Get().SetAllUserFocus(Box, EFocusCause::SetDirectly);
		}

		void SetText(const FString& Text) { Box->SetText(FText::FromString(Text)); }

		void Send()
		{
			const FString Text = Box->GetText().ToString().TrimStartAndEnd();
			if (bSending || Text.IsEmpty())
			{
				return;
			}
			FStudioDaemonInfo Info;
			if (!FStudioDaemon::ReadLive(Info))
			{
				Error = LOCTEXT("NoDaemon", "Studio is not running. Open Studio once, then ask again.");
				return;
			}
			bSending = true;
			Error = FText::GetEmpty();
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/ext/studio/quick"), Info.Port));
			Request->SetVerb(TEXT("POST"));
			Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Info.Token);
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetContentAsString(FStudioQuickAsk::RequestBody(Text, Context));
			TWeakPtr<SPill> Weak = StaticCastSharedRef<SPill>(AsShared());
			// The request runs on the HTTP thread; this answer arrives back on the game thread.
			Request->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
			{
				const int32 Code = bOk && Response.IsValid() ? Response->GetResponseCode() : 0;
				if (Code == 201 || Code == 200)
				{
					UE_LOG(LogStudioQuickAsk, Log, TEXT("quick ask: %s"), *Response->GetContentAsString());
					FStudioQuickAsk::Close();
					FStudioDock::OpenStudio();
					return;
				}
				FText Message = FText::Format(LOCTEXT("Failed", "Studio did not take the message ({0})."), FText::AsNumber(Code));
				TSharedPtr<FJsonObject> Body;
				if (Response.IsValid() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Body) && Body.IsValid() && Body->HasTypedField<EJson::String>(TEXT("error")))
				{
					Message = FText::FromString(Body->GetStringField(TEXT("error")));
				}
				if (Code == 404)
				{
					Message = LOCTEXT("TooOld", "This Studio has no quick ask. Update Studio to 0.1.2-beta.12 or later.");
				}
				else if (Code == 400 && Message.ToString().Contains(TEXT("context kind")))
				{
					Message = LOCTEXT("NoLocation", "This Studio does not take the point under the cursor. Update Studio to 0.1.2-beta.19 or later.");
				}
				UE_LOG(LogStudioQuickAsk, Warning, TEXT("quick ask answered %d: %s"), Code, *Message.ToString());
				if (TSharedPtr<SPill> P = Weak.Pin())
				{
					P->bSending = false;
					P->Error = Message;
					P->FocusBox();
				}
			});
			Request->ProcessRequest();
		}

		virtual bool SupportsKeyboardFocus() const override { return true; }

		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& Key) override
		{
			if (Key.GetKey() == EKeys::Escape)
			{
				FStudioQuickAsk::Close();
			}
			return FReply::Handled();
		}
		virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }

		// Dragged by anything that is not the text box or Send, which take their own clicks first.
		virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent& Mouse) override
		{
			if (Mouse.GetEffectingButton() != EKeys::LeftMouseButton || !PillWindow.IsValid())
			{
				return FReply::Unhandled();
			}
			DragOffset = Mouse.GetScreenSpacePosition() - PillWindow->GetPositionInScreen();
			return FReply::Handled().CaptureMouse(AsShared());
		}

		virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent& Mouse) override
		{
			if (!HasMouseCapture() || !PillWindow.IsValid())
			{
				return FReply::Unhandled();
			}
			PillWindow->MoveWindowTo(Mouse.GetScreenSpacePosition() - DragOffset);
			return FReply::Handled();
		}

		virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& Mouse) override
		{
			return HasMouseCapture() && Mouse.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled().ReleaseMouseCapture() : FReply::Unhandled();
		}

		virtual FCursorReply OnCursorQuery(const FGeometry&, const FPointerEvent&) const override
		{
			return FCursorReply::Cursor(HasMouseCapture() ? EMouseCursor::GrabHandClosed : EMouseCursor::GrabHand);
		}
		virtual FReply OnKeyChar(const FGeometry&, const FCharacterEvent&) override { return FReply::Handled(); }

		bool bSending = false;
		FText Error;

	private:
		FVector2D DragOffset = FVector2D::ZeroVector;

		TSharedRef<SWidget> Chip(const FText& Text, const FText& Tooltip)
		{
			return SNew(SBorder)
				.BorderImage(ChipBrush())
				.Padding(FMargin(8.0f, 2.0f))
				.ToolTipText(Tooltip)
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).Text(Text)
				];
		}

		FReply OnBoxKey(const FGeometry&, const FKeyEvent& Key)
		{
			if (Key.GetKey() == EKeys::Escape)
			{
				FStudioQuickAsk::Close();
				return FReply::Handled();
			}
			return FReply::Unhandled();
		}

		void OnCommitted(const FText&, ETextCommit::Type How)
		{
			if (How == ETextCommit::OnEnter)
			{
				Send();
			}
		}

		TArray<FStudioQuickContext> Context;
		TSharedPtr<SEditableTextBox> Box;
	};

	/** Open on the next tick, once the menu or input event that asked has finished with focus. */
	void OpenSoon(const TArray<FStudioQuickContext>& Context)
	{
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Context](float)
		{
			FStudioQuickAsk::Open(Context);
			return false;
		}));
	}

	/** The level viewport under a screen point, if there is one. */
	TSharedPtr<SLevelViewport> LevelViewportAt(FSlateApplication& App, const FVector2D& Screen)
	{
		const FWidgetPath Path = App.LocateWindowUnderMouse(Screen, App.GetInteractiveTopLevelWindows());
		static const FName LevelViewport(TEXT("SLevelViewport"));
		for (int32 i = Path.Widgets.Num() - 1; i >= 0; --i)
		{
			if (Path.Widgets[i].Widget->GetType() == LevelViewport)
			{
				return StaticCastSharedRef<SLevelViewport>(Path.Widgets[i].Widget);
			}
		}
		return nullptr;
	}

	/**
	 * The world point under the cursor in a level viewport, as a "location" item:
	 * the hit point and surface normal, the actor hit and the map. A trace that
	 * hits nothing gives no item, rather than a point in the air.
	 */
	bool PointUnderCursor(FSlateApplication& App, const FVector2D& Screen, FStudioQuickContext& Out)
	{
		const TSharedPtr<SLevelViewport> Level = LevelViewportAt(App, Screen);
		if (!Level.IsValid())
		{
			return false;
		}
		FLevelEditorViewportClient& Client = Level->GetLevelViewportClient();
		const TSharedPtr<SViewport> Widget = Level->GetViewportWidget().Pin();
		UWorld* World = Client.GetWorld();
		if (!Client.Viewport || !Widget.IsValid() || !World)
		{
			return false;
		}
		const FGeometry& Geometry = Widget->GetTickSpaceGeometry();
		const FVector2D Local = Geometry.AbsoluteToLocal(Screen);
		const FVector2D LocalSize = Geometry.GetLocalSize();
		const FIntPoint Pixels = Client.Viewport->GetSizeXY();
		if (LocalSize.X <= 0 || LocalSize.Y <= 0 || Local.X < 0 || Local.Y < 0 || Local.X > LocalSize.X || Local.Y > LocalSize.Y)
		{
			return false;
		}
		const FVector2D Pixel(Local.X / LocalSize.X * Pixels.X, Local.Y / LocalSize.Y * Pixels.Y);
		FSceneViewFamilyContext Family(FSceneViewFamily::ConstructionValues(Client.Viewport, Client.GetScene(), Client.EngineShowFlags).SetRealtimeUpdate(true));
		const FSceneView* View = Client.CalcSceneView(&Family);
		if (!View)
		{
			return false;
		}
		FVector Origin, Direction;
		View->DeprojectFVector2D(Pixel, Origin, Direction);
		FHitResult Hit;
		const FCollisionQueryParams Query(SCENE_QUERY_STAT(StudioQuickAsk), true);
		if (!World->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * HALF_WORLD_MAX, ECC_Visibility, Query))
		{
			return false;
		}
		Out.Kind = TEXT("location");
		Out.Path = FString::Printf(TEXT("X=%.1f Y=%.1f Z=%.1f, surface normal X=%.2f Y=%.2f Z=%.2f"),
			Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z, Hit.ImpactNormal.X, Hit.ImpactNormal.Y, Hit.ImpactNormal.Z);
		Out.Label = Hit.GetActor() ? Hit.GetActor()->GetActorLabel() : FString();
		Out.Class = World->GetOutermost()->GetName();
		return true;
	}

	/** What a hotkey or gesture over the level viewport carries: the point under the cursor, then any selected actors. */
	TArray<FStudioQuickContext> ViewportContext(FSlateApplication& App, const FVector2D& Screen, bool bSelection)
	{
		TArray<FStudioQuickContext> Out;
		FStudioQuickContext Point;
		if (PointUnderCursor(App, Screen, Point))
		{
			Out.Add(Point);
		}
		if (bSelection && LevelViewportAt(App, Screen).IsValid())
		{
			Out.Append(FStudioQuickAsk::ContextOfSelectedActors());
		}
		return Out;
	}

	/**
	 * The hotkey, wherever focus is, and Ctrl+Alt+right-click. The click is
	 * consumed, press and release, so no context menu opens and the camera stays.
	 */
	class FInput : public IInputProcessor
	{
	public:
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Key) override
		{
			const TSharedPtr<FUICommandInfo>& Command = FStudioQuickAskCommands::Get().AskStudio;
			const FInputChord Chord(Key.GetKey(), EModifierKey::FromBools(Key.IsControlDown(), Key.IsAltDown(), Key.IsShiftDown(), Key.IsCommandDown()));
			if (!Command.IsValid() || !Command->HasActiveChord(Chord))
			{
				return false;
			}
			if (!Key.IsRepeat())
			{
				OpenSoon(ViewportContext(FSlateApplication::Get(), FSlateApplication::Get().GetCursorPos(), false));
			}
			return true;
		}

		virtual bool HandleMouseButtonDownEvent(FSlateApplication& App, const FPointerEvent& Mouse) override
		{
			if (!IsGesture(Mouse))
			{
				return false;
			}
			bSwallowUp = true;
			OpenSoon(ViewportContext(App, Mouse.GetScreenSpacePosition(), true));
			return true;
		}

		virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication&, const FPointerEvent& Mouse) override
		{
			return IsGesture(Mouse);
		}

		virtual bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent& Mouse) override
		{
			if (bSwallowUp && Mouse.GetEffectingButton() == EKeys::RightMouseButton)
			{
				bSwallowUp = false;
				return true;
			}
			return false;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("UEMCPStudioQuickAsk"); }

	private:
		static bool IsGesture(const FPointerEvent& Mouse)
		{
			const FModifierKeysState& Mods = Mouse.GetModifierKeys();
			return Mouse.GetEffectingButton() == EKeys::RightMouseButton && Mods.IsControlDown() && Mods.IsAltDown() && !Mods.IsShiftDown() && !Mods.IsCommandDown();
		}

		bool bSwallowUp = false;
	};

	TSharedRef<FExtender> ExtendActorMenu(const TSharedRef<FUICommandList>, const TArray<AActor*> Actors)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		if (Actors.Num() == 0)
		{
			return Extender;
		}
		const TArray<FStudioQuickContext> Context = FStudioQuickAsk::ContextOfActors(Actors);
		Extender->AddMenuExtension("ActorControl", EExtensionHook::After, nullptr, FMenuExtensionDelegate::CreateLambda([Context](FMenuBuilder& Menu)
		{
			Menu.AddMenuEntry(
				LOCTEXT("ChatAboutActors", "Chat about this"),
				LOCTEXT("ChatAboutActorsTip", "Ask UE-MCP Studio about the selected actors in a new session"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Context] { OpenSoon(Context); })));
		}));
		return Extender;
	}

	void ExtendAssetMenu()
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("CommonAssetActions");
		Section.AddDynamicEntry("UEMCPStudioChatAbout", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UContentBrowserAssetContextMenuContext* Ctx = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
			if (!Ctx || Ctx->SelectedAssets.Num() == 0)
			{
				return;
			}
			const TArray<FStudioQuickContext> Context = FStudioQuickAsk::ContextOfAssets(Ctx->SelectedAssets);
			InSection.AddMenuEntry(
				"UEMCPStudioChatAboutAssets",
				LOCTEXT("ChatAboutAssets", "Chat about this"),
				LOCTEXT("ChatAboutAssetsTip", "Ask UE-MCP Studio about the selected assets in a new session"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([Context] { OpenSoon(Context); })));
		}));
	}

	FString JoinArgs(const TArray<FString>& Args)
	{
		return FString::Join(Args, TEXT(" "));
	}
}

void FStudioQuickAsk::Startup()
{
	using namespace StudioQuickAsk;
	FStudioQuickAskCommands::Register();
	if (FSlateApplication::IsInitialized())
	{
		Input = MakeShared<FInput>();
		FSlateApplication::Get().RegisterInputPreProcessor(Input);
	}

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	// The level editor's outliner builds its actor menu from the same extenders, so this covers both.
	auto& Extenders = LevelEditor.GetAllLevelViewportContextMenuExtenders();
	Extenders.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&ExtendActorMenu));
	ActorMenuHandle = Extenders.Last().GetHandle();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
	{
		FToolMenuOwnerScoped Owner(TEXT("UEMCPStudioQuickAsk"));
		ExtendAssetMenu();
	}));

	IConsoleManager& Console = IConsoleManager::Get();
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk"),
		TEXT("Open the Ask Studio pill at the cursor, as the hotkey does. Studio.QuickAsk [text]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { FStudioQuickAsk::Open({}, JoinArgs(Args)); })));
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk.Selection"),
		TEXT("Open the pill about the selected actors, as Chat about this does in the viewport and outliner. Studio.QuickAsk.Selection [text]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args) { FStudioQuickAsk::Open(FStudioQuickAsk::ContextOfSelectedActors(), JoinArgs(Args)); })));
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk.Assets"),
		TEXT("Open the pill about the Content Browser's selected assets, as Chat about this does there. Studio.QuickAsk.Assets [text]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			TArray<FAssetData> Assets;
			FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Assets);
			FStudioQuickAsk::Open(FStudioQuickAsk::ContextOfAssets(Assets), JoinArgs(Args));
		})));
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk.Send"),
		TEXT("Send what the open pill holds, as Enter does."),
		FConsoleCommandDelegate::CreateLambda([] { if (!FStudioQuickAsk::SendOpen()) UE_LOG(LogStudioQuickAsk, Warning, TEXT("no Ask Studio pill is open")); })));
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk.Close"),
		TEXT("Close the Ask Studio pill, as Esc does."),
		FConsoleCommandDelegate::CreateStatic(&FStudioQuickAsk::Close)));
}

void FStudioQuickAsk::Shutdown()
{
	using namespace StudioQuickAsk;
	Close();
	for (IConsoleObject* Command : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Command);
	}
	ConsoleCommands.Empty();
	if (Input.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(Input);
	}
	Input.Reset();
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		const FDelegateHandle Handle = ActorMenuHandle;
		LevelEditor->GetAllLevelViewportContextMenuExtenders().RemoveAll([Handle](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& D) { return D.GetHandle() == Handle; });
	}
	if (UToolMenus::IsToolMenuUIEnabled())
	{
		UToolMenus::UnregisterOwner(TEXT("UEMCPStudioQuickAsk"));
	}
	FStudioQuickAskCommands::Unregister();
}

namespace StudioQuickAsk
{
	/** Centred under the cursor, kept on the monitor's work area, in screen pixels. */
	FVector2D PlaceAtCursor(const FVector2D& Size)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const FVector2D Cursor = App.GetCursorPos();
		const float Dpi = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(Cursor.X, Cursor.Y);
		const FSlateRect Work = App.GetWorkArea(FSlateRect(Cursor, Cursor));
		const float Margin = 8.0f * Dpi;
		FVector2D Pos(Cursor.X - Size.X * 0.5, Cursor.Y + 16.0 * Dpi);
		if (Pos.Y + Size.Y > Work.Bottom - Margin)
		{
			Pos.Y = Cursor.Y - 16.0 * Dpi - Size.Y;
		}
		Pos.X = FMath::Clamp(Pos.X, Work.Left + Margin, FMath::Max(Work.Left + Margin, Work.Right - Margin - Size.X));
		Pos.Y = FMath::Clamp(Pos.Y, Work.Top + Margin, FMath::Max(Work.Top + Margin, Work.Bottom - Margin - Size.Y));
		return Pos;
	}

	/**
	 * Clip the native window to the pill's rounded outline. Per-pixel transparency
	 * is not reliable across RHIs and draws the corners black; a window region
	 * leaves nothing there to draw. Kept in step with the window's size.
	 */
	void TrackShape()
	{
#if PLATFORM_WINDOWS
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Shaped = FIntPoint(0, 0)](float) mutable
		{
			if (!PillWindow.IsValid())
			{
				return false;
			}
			const TSharedPtr<FGenericWindow> Native = PillWindow->GetNativeWindow();
			HWND Hwnd = Native.IsValid() ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
			RECT R;
			if (!Hwnd || !::GetWindowRect(Hwnd, &R))
			{
				return true;
			}
			const FIntPoint Size(R.right - R.left, R.bottom - R.top);
			if (Size != Shaped && Size.X > 0 && Size.Y > 0)
			{
				Shaped = Size;
				const int32 D = FMath::RoundToInt(2.0f * PillRadius * Native->GetDPIScaleFactor());
				::SetWindowRgn(Hwnd, ::CreateRoundRectRgn(0, 0, Size.X + 1, Size.Y + 1, D, D), 1);
			}
			return true;
		}));
#endif
	}
}

void FStudioQuickAsk::Open(const TArray<FStudioQuickContext>& Context, const FString& Text)
{
	using namespace StudioQuickAsk;
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	FSlateApplication& App = FSlateApplication::Get();
	if (TSharedPtr<SPill> Existing = Pill.Pin(); Existing.IsValid() && PillWindow.IsValid() && Context.Num() == 0)
	{
		// Asked again while open: keep what is there, and bring it back.
		if (!Text.IsEmpty())
		{
			Existing->SetText(Text);
		}
		PillWindow->MoveWindowTo(PlaceAtCursor(PillWindow->GetSizeInScreen()));
		PillWindow->BringToFront();
		Existing->FocusBox();
		return;
	}
	Close();

	TSharedRef<SPill> Widget = SNew(SPill).Context(Context).Text(Text);
	Widget->SlatePrepass(App.GetApplicationScale());
	const FVector2D Cursor = App.GetCursorPos();
	const FVector2D Size = Widget->GetDesiredSize() * FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(Cursor.X, Cursor.Y);
	const FVector2D Pos = PlaceAtCursor(Size);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Type(EWindowType::Menu)
		.Title(LOCTEXT("PillTitle", "Ask Studio"))
		.IsPopupWindow(true)
		.CreateTitleBar(false)
		.HasCloseButton(false)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::Autosized)
		.SupportsTransparency(EWindowTransparency::PerPixel)
		.ActivationPolicy(EWindowActivationPolicy::Always)
		.FocusWhenFirstShown(true)
		.ScreenPosition(Pos)
		// Pos is already in screen pixels; Slate would scale it by the DPI again and miss the cursor.
		.AdjustInitialSizeAndPositionForDPIScale(false)
		[
			Widget
		];
	Window->GetOnWindowDeactivatedEvent().AddLambda([]
	{
		// Clicking away closes it. Right after opening, the window that asked can still take activation back.
		if (FPlatformTime::Seconds() - OpenedAt < SettleSeconds)
		{
			if (PillWindow.IsValid())
			{
				PillWindow->BringToFront(true);
			}
			if (TSharedPtr<SPill> P = Pill.Pin())
			{
				P->FocusBox();
			}
			return;
		}
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) { FStudioQuickAsk::Close(); return false; }));
	});

	PillWindow = Window;
	Pill = Widget;
	OpenedAt = FPlatformTime::Seconds();
	TSharedPtr<SWindow> Parent = App.GetActiveTopLevelRegularWindow();
	if (!Parent.IsValid())
	{
		Parent = FGlobalTabmanager::Get()->GetRootWindow();
	}
	if (Parent.IsValid())
	{
		App.AddWindowAsNativeChild(Window, Parent.ToSharedRef(), true);
	}
	else
	{
		App.AddWindow(Window, true);
	}
	Window->BringToFront(true);
	Widget->FocusBox();
	TrackShape();
}

bool FStudioQuickAsk::SendOpen()
{
	if (TSharedPtr<StudioQuickAsk::SPill> P = StudioQuickAsk::Pill.Pin())
	{
		P->Send();
		return true;
	}
	return false;
}

void FStudioQuickAsk::Close()
{
	using namespace StudioQuickAsk;
	Pill.Reset();
	if (TSharedPtr<SWindow> Window = PillWindow)
	{
		PillWindow.Reset();
		Window->RequestDestroyWindow();
	}
}

TArray<FStudioQuickContext> FStudioQuickAsk::ContextOfActors(const TArray<AActor*>& Actors)
{
	TArray<FStudioQuickContext> Out;
	for (const AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Out.Add({ TEXT("actor"), Actor->GetActorLabel(), Actor->GetPathName(), Actor->GetClass()->GetName() });
		}
	}
	return Out;
}

TArray<FStudioQuickContext> FStudioQuickAsk::ContextOfAssets(const TArray<FAssetData>& Assets)
{
	TArray<FStudioQuickContext> Out;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.IsValid())
		{
			Out.Add({ TEXT("asset"), Asset.AssetName.ToString(), Asset.GetObjectPathString(), Asset.AssetClassPath.GetAssetName().ToString() });
		}
	}
	return Out;
}

TArray<FStudioQuickContext> FStudioQuickAsk::ContextOfSelectedActors()
{
	TArray<AActor*> Actors;
	if (GEditor)
	{
		GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Actors);
	}
	return ContextOfActors(Actors);
}

FString FStudioQuickAsk::RequestBody(const FString& Text, const TArray<FStudioQuickContext>& Context)
{
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("text"), Text);
	if (Context.Num())
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FStudioQuickContext& C : Context)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("kind"), C.Kind);
			Item->SetStringField(TEXT("label"), C.Label);
			Item->SetStringField(TEXT("path"), C.Path);
			if (!C.Class.IsEmpty())
			{
				Item->SetStringField(TEXT("class"), C.Class);
			}
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
		Body->SetArrayField(TEXT("context"), Items);
	}
	FString Out;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
	return Out;
}

#undef LOCTEXT_NAMESPACE
