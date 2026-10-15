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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "LevelEditorViewport.h"
#include "SceneView.h"
#include "SLevelViewport.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "SGraphNode.h"
#include "SGraphPanel.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/SViewport.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "ToolMenus.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Styling/SlateStyle.h"

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

	/** The corner nearest the cursor, square so the pill points at what it is about: 0 top left, 1 top right, 2 bottom right, 3 bottom left. */
	int32 PointCorner = 0;

	FVector4 PillRadii(int32 Square)
	{
		FVector4 R(PillRadius, PillRadius, PillRadius, PillRadius);
		R[Square] = 0.0f;
		return R;
	}

	/**
	 * The pill's colours for the theme picked in Studio's Settings: Studio's own
	 * palette for ue-mcp, Unreal's style for Editor, and Unreal's colours with the
	 * ue-mcp orange for Editor orange. Outlines are
	 * Studio's translucent lines laid over the panel, so they stay opaque here.
	 */
	struct FPillTheme
	{
		bool bEditor = true;
		FSlateColor Panel, Outline, Input, Text, Dim, Accent, AccentHi, OnAccent;
	};

	FLinearColor Hex(const TCHAR* H) { return FLinearColor(FColor::FromHex(H)); }

	/** The theme in the project's Studio settings, Editor when there is none. */
	FPillTheme ReadTheme()
	{
		FString Json;
		const FString Path = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(PLATFORM_WINDOWS ? TEXT("USERPROFILE") : TEXT("HOME")),
			TEXT(".ue-mcp"), TEXT("data"), FStudioDaemon::ProjectKey(), TEXT("studio"), TEXT("settings.json"));
		TSharedPtr<FJsonObject> Settings;
		FString Name = TEXT("ue-mcp");
		if (FFileHelper::LoadFileToString(Json, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Settings) && Settings.IsValid())
		{
			Settings->TryGetStringField(TEXT("theme"), Name);
		}
		FPillTheme T;
		if (Name == TEXT("editor-orange"))
		{
			T.bEditor = false;
			T.Panel = FStyleColors::Panel;
			T.Outline = FStyleColors::InputOutline;
			T.Input = FStyleColors::Input;
			T.Text = FSlateColor::UseForeground();
			T.Dim = FSlateColor::UseSubduedForeground();
			T.Accent = Hex(TEXT("e8720c"));
			T.AccentHi = Hex(TEXT("f28a2e"));
			T.OnAccent = Hex(TEXT("140a02"));
		}
		else if (Name != TEXT("editor"))
		{
			T.bEditor = false;
			T.Panel = Hex(TEXT("131312"));
			T.Outline = Hex(TEXT("2f2f2d"));
			T.Input = Hex(TEXT("0c0c0b"));
			T.Text = Hex(TEXT("ece9e3"));
			T.Dim = Hex(TEXT("7b7871"));
			T.Accent = Hex(TEXT("e8720c"));
			T.AccentHi = Hex(TEXT("f28a2e"));
			T.OnAccent = Hex(TEXT("140a02"));
		}
		else
		{
			T.Panel = FStyleColors::Panel;
			T.Outline = FStyleColors::InputOutline;
			T.Input = FStyleColors::Input;
		}
		return T;
	}

	FText ChipText(const FStudioQuickContext& C)
	{
		if (C.Kind == TEXT("graph"))
		{
			return FText::Format(LOCTEXT("GraphChip", "In {0}"), FText::FromString(C.Label));
		}
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
			Theme = ReadTheme();
			for (int32 i = 0; i < 4; ++i)
			{
				PillBrushes.Add(FSlateRoundedBoxBrush(Theme.Panel, PillRadii(i), Theme.Outline, 1.0f));
			}
			ChipBrush = FSlateRoundedBoxBrush(Theme.Input, 9.0f, Theme.Outline, 1.0f);
			BoxStyle = FAppStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
			SendStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton");
			if (!Theme.bEditor)
			{
				BoxNormal = FSlateRoundedBoxBrush(Theme.Input, 6.0f, Theme.Outline, 1.0f);
				BoxFocused = FSlateRoundedBoxBrush(Theme.Input, 6.0f, Theme.Accent, 1.0f);
				BoxStyle.SetBackgroundImageNormal(BoxNormal).SetBackgroundImageHovered(BoxNormal).SetBackgroundImageFocused(BoxFocused).SetBackgroundImageReadOnly(BoxNormal)
					.SetForegroundColor(Theme.Text).SetFocusedForegroundColor(Theme.Text);
				SendNormal = FSlateRoundedBoxBrush(Theme.Accent, 6.0f);
				SendHovered = FSlateRoundedBoxBrush(Theme.AccentHi, 6.0f);
				SendStyle.SetNormal(SendNormal).SetHovered(SendHovered).SetPressed(SendNormal)
					.SetNormalForeground(Theme.OnAccent).SetHoveredForeground(Theme.OnAccent).SetPressedForeground(Theme.OnAccent);
			}
			// The reply's rich text: <b> bold, <c> code, in the theme's text colour.
			BodyStyle = FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText");
			if (!Theme.bEditor)
			{
				BodyStyle.SetColorAndOpacity(Theme.Text);
			}
			RichStyle = MakeShared<FSlateStyleSet>(TEXT("UEMCPStudioPillText"));
			RichStyle->Set("b", FTextBlockStyle(BodyStyle).SetFont(FCoreStyle::GetDefaultFontStyle("Bold", BodyStyle.Font.Size)));
			RichStyle->Set("c", FTextBlockStyle(BodyStyle).SetFont(FCoreStyle::GetDefaultFontStyle("Mono", BodyStyle.Font.Size - 1)));
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
				.BorderImage_Lambda([this] { return &PillBrushes[PointCorner & 3]; })
				.Padding(FMargin(14.0f, 8.0f, 8.0f, 6.0f))
				[
					SNew(SBox)
					.WidthOverride(440.0f)
					[
						SAssignNew(Pages, SWidgetSwitcher)
						+ SWidgetSwitcher::Slot()
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
								.Style(&BoxStyle)
								.Text(FText::FromString(InArgs._Text))
								.HintText(Context.Num() ? LOCTEXT("HintContext", "Ask about this") : LOCTEXT("Hint", "Ask Studio"))
								.IsEnabled_Lambda([this] { return !bSending; })
								.OnTextCommitted(this, &SPill::OnCommitted)
								.OnKeyDownHandler(this, &SPill::OnBoxKey)
							]
							+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f).VAlign(VAlign_Center)
							[
								SNew(SButton)
								.ButtonStyle(&SendStyle)
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
							.ColorAndOpacity_Lambda([this] { return !Error.IsEmpty() ? FSlateColor(FStyleColors::Error) : Theme.bEditor ? FSlateColor::UseSubduedForeground() : FSlateColor(Theme.Dim); })
							.Text_Lambda([this] { return Error.IsEmpty() ? LOCTEXT("KeysHint", "Enter to send, Esc to close") : Error; })
						]
						]
						+ SWidgetSwitcher::Slot()
						[
							ThreadPage()
						]
					]
				]
			];
		}

		virtual ~SPill() override
		{
			FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		}

		/** Once a thread runs, clicking elsewhere leaves the pill open, so the editor can be watched while the agent works. */
		bool IsThread() const { return !SessionId.IsEmpty(); }

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
			if (!FStudioDaemon::ReadReady(Info))
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
					TSharedPtr<FJsonObject> Created;
					FString Id;
					if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Created) && Created.IsValid() && Created->TryGetStringField(TEXT("sessionId"), Id))
					{
						if (TSharedPtr<SPill> P = Weak.Pin())
						{
							P->StartThread(Id);
							return;
						}
					}
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
					Message = LOCTEXT("NoLocation", "This Studio does not take what is under the cursor. Update Studio to 0.1.2-beta.20 or later.");
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

		/** Show the session's thread in place of the prompt, and follow it until the turn ends. */
		void StartThread(const FString& Id)
		{
			SessionId = Id;
			Asked = Box->GetText().ToString().TrimStartAndEnd();
			bSending = false;
			Status = TEXT("running");
			Pages->SetActiveWidgetIndex(1);
			FSlateApplication::Get().SetAllUserFocus(AsShared(), EFocusCause::SetDirectly);
			Poll();
			TWeakPtr<SPill> Weak = StaticCastSharedRef<SPill>(AsShared());
			PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
			{
				TSharedPtr<SPill> P = Weak.Pin();
				if (!P.IsValid())
				{
					return false;
				}
				P->Poll();
				return P->IsLive();
			}), 0.5f);
		}

	private:
		FVector2D DragOffset = FVector2D::ZeroVector;

		// The thread: the session the pill started and the turn as Studio last described it.
		FString SessionId;
		FString Asked;
		FString Status;
		FString Reply;
		FString NeedsYou;
		FString ThreadError;
		bool bPolling = false;
		struct FStep
		{
			FString Label;
			FString State;
		};
		/** The steps /quick listed, newest last, and how many came before them. */
		TArray<FStep> Steps;
		int32 Earlier = 0;
		/** Every main-thread call's label from the transcript, oldest first; empty where it has no rule. */
		TArray<FString> Labels;
		bool bFetchingLabels = false;
		FTSTicker::FDelegateHandle PollHandle;
		TSharedPtr<SWidgetSwitcher> Pages;
		TSharedPtr<SVerticalBox> StepsBox;
		FTextBlockStyle BodyStyle;
		TSharedPtr<FSlateStyleSet> RichStyle;

		bool IsLive() const { return Status == TEXT("running") || Status == TEXT("waiting"); }

		FSlateColor TextColor() const { return Theme.bEditor ? FSlateColor::UseForeground() : FSlateColor(Theme.Text); }
		FSlateColor DimColor() const { return Theme.bEditor ? FSlateColor::UseSubduedForeground() : FSlateColor(Theme.Dim); }

		/** One call to Studio; the callback runs on the game thread with the status code and body. */
		void Call(const FString& Verb, const FString& Path, TFunction<void(int32, const FString&)> Done)
		{
			FStudioDaemonInfo Info;
			if (!FStudioDaemon::ReadLive(Info))
			{
				Done(0, FString());
				return;
			}
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/ext/studio%s"), Info.Port, *Path));
			Request->SetVerb(Verb);
			Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Info.Token);
			if (Verb == TEXT("POST"))
			{
				Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
				Request->SetContentAsString(TEXT("{}"));
			}
			Request->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
			{
				Done(bOk && Response.IsValid() ? Response->GetResponseCode() : 0, Response.IsValid() ? Response->GetContentAsString() : FString());
			});
			Request->ProcessRequest();
		}

		void Poll()
		{
			if (bPolling || SessionId.IsEmpty())
			{
				return;
			}
			bPolling = true;
			TWeakPtr<SPill> Weak = StaticCastSharedRef<SPill>(AsShared());
			Call(TEXT("GET"), TEXT("/quick/") + SessionId, [Weak](int32 Code, const FString& Body)
			{
				TSharedPtr<SPill> P = Weak.Pin();
				if (!P.IsValid())
				{
					return;
				}
				P->bPolling = false;
				TSharedPtr<FJsonObject> T;
				if (Code == 200 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), T) && T.IsValid())
				{
					P->Apply(*T);
				}
				else if (Code == 404)
				{
					// A Studio older than the thread view: the session runs, it just cannot be shown here.
					P->ThreadError = TEXT("This Studio cannot show the thread here. Continue it in Studio.");
					P->Status = TEXT("idle");
				}
			});
		}

		void Apply(const FJsonObject& T)
		{
			T.TryGetStringField(TEXT("status"), Status);
			FString Text;
			// Studio lays the reply out as rich text; an older Studio sends only the markdown.
			Reply = FStudioQuickAsk::TablesAsRows(T.TryGetStringField(TEXT("replyMarkup"), Text) ? Text : T.TryGetStringField(TEXT("reply"), Text) ? Escaped(Plain(Text)) : FString());
			NeedsYou = T.TryGetStringField(TEXT("needsYou"), Text) ? Text : FString();
			ThreadError = T.TryGetStringField(TEXT("error"), Text) ? Text : FString();
			Earlier = T.HasTypedField<EJson::Number>(TEXT("earlier")) ? static_cast<int32>(T.GetNumberField(TEXT("earlier"))) : 0;
			Steps.Reset();
			const TArray<TSharedPtr<FJsonValue>>* StepValues = nullptr;
			if (T.TryGetArrayField(TEXT("steps"), StepValues))
			{
				for (const TSharedPtr<FJsonValue>& V : *StepValues)
				{
					const TSharedPtr<FJsonObject>* Step = nullptr;
					if (V.IsValid() && V->TryGetObject(Step))
					{
						Steps.Add({ (*Step)->GetStringField(TEXT("label")), (*Step)->GetStringField(TEXT("state")) });
					}
				}
			}
			ShowSteps();
			if (Earlier + Steps.Num() > Labels.Num())
			{
				FetchLabels();
			}
		}

		/**
		 * Studio's /quick names ue-mcp calls other than "call" by the tool alone ("ue_mcp"). The
		 * session's transcript has each call's input, so the pill labels them as the panel does;
		 * it is fetched again only when a step appears that has no label yet.
		 */
		void FetchLabels()
		{
			if (bFetchingLabels)
			{
				return;
			}
			bFetchingLabels = true;
			TWeakPtr<SPill> Weak = StaticCastSharedRef<SPill>(AsShared());
			Call(TEXT("GET"), TEXT("/sessions/") + SessionId, [Weak](int32 Code, const FString& Body)
			{
				TSharedPtr<SPill> P = Weak.Pin();
				if (!P.IsValid())
				{
					return;
				}
				P->bFetchingLabels = false;
				if (Code == 200)
				{
					P->Labels = FStudioQuickAsk::MainStepLabels(Body);
					P->ShowSteps();
				}
			});
		}

		void ShowSteps()
		{
			StepsBox->ClearChildren();
			if (Earlier > 0)
			{
				StepsBox->AddSlot().AutoHeight()
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(DimColor())
					.Text(FText::Format(LOCTEXT("EarlierSteps", "{0} earlier steps"), FText::AsNumber(Earlier)))
				];
			}
			// The transcript's calls line up with the steps only when it holds every one of them.
			const bool bLabelled = Labels.Num() == Earlier + Steps.Num();
			for (int32 i = 0; i < Steps.Num(); ++i)
			{
				const FString& State = Steps[i].State;
				const FString Own = bLabelled ? Labels[Earlier + i] : FString();
				const FString& Label = Own.IsEmpty() ? Steps[i].Label : Own;
				const FSlateColor StateColor = State == TEXT("failed") ? FSlateColor(FStyleColors::Error)
					: State == TEXT("running") ? (Theme.bEditor ? FSlateColor(FStyleColors::Primary) : FSlateColor(Theme.Accent))
					: DimColor();
				StepsBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(TextColor()).Text(FText::FromString(Label))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).ColorAndOpacity(StateColor).Text(FText::FromString(State))
					]
				];
			}
		}

		/** Text that rich text markup shows as written. */
		static FString Escaped(FString Text)
		{
			Text.ReplaceInline(TEXT("&"), TEXT("&amp;"));
			Text.ReplaceInline(TEXT("<"), TEXT("&lt;"));
			Text.ReplaceInline(TEXT(">"), TEXT("&gt;"));
			return Text;
		}

		/** Markdown read as text: emphasis and code marks dropped. */
		static FString Plain(FString Text)
		{
			Text.ReplaceInline(TEXT("**"), TEXT(""));
			Text.ReplaceInline(TEXT("`"), TEXT(""));
			return Text.TrimStartAndEnd();
		}

		FText StatusLine() const
		{
			if (!ThreadError.IsEmpty())
			{
				return FText::FromString(ThreadError);
			}
			if (!NeedsYou.IsEmpty())
			{
				return FText::Format(LOCTEXT("NeedsYou", "Waiting for you: {0} Continue in Studio to answer."), FText::FromString(NeedsYou));
			}
			if (Status == TEXT("running"))
			{
				return LOCTEXT("Working", "Working. Closing this leaves it running in Studio.");
			}
			if (Status == TEXT("stopped"))
			{
				return LOCTEXT("Stopped", "Stopped.");
			}
			return LOCTEXT("Done", "Done. Continue in Studio to follow up.");
		}

		TSharedRef<SWidget> ThreadPage()
		{
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(DimColor()).Text_Lambda([this] { return FText::FromString(Asked); })
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SAssignNew(StepsBox, SVerticalBox)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).MaxDesiredHeight(280.0f).Visibility_Lambda([this] { return Reply.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SRichTextBlock).AutoWrapText(true).TextStyle(&BodyStyle).DecoratorStyleSet(RichStyle.Get())
							.Text_Lambda([this] { return FText::FromString(Reply); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).AutoWrapText(true)
					.ColorAndOpacity_Lambda([this] { return !ThreadError.IsEmpty() ? FSlateColor(FStyleColors::Error) : !NeedsYou.IsEmpty() ? FSlateColor(FStyleColors::Warning) : DimColor(); })
					.Text_Lambda([this] { return StatusLine(); })
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("StopThread", "Stop"))
						.Visibility_Lambda([this] { return Status == TEXT("running") ? EVisibility::Visible : EVisibility::Collapsed; })
						.OnClicked_Lambda([this]
						{
							Call(TEXT("POST"), FString::Printf(TEXT("/sessions/%s/stop"), *SessionId), [](int32, const FString&) {});
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSpacer)]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.ButtonStyle(&SendStyle)
						.Text(LOCTEXT("ContinueThread", "Continue this thread"))
						.OnClicked_Lambda([this]
						{
							Call(TEXT("POST"), FString::Printf(TEXT("/quick/%s/continue"), *SessionId), [](int32, const FString&)
							{
								FStudioDock::OpenStudio();
								FStudioQuickAsk::Close();
							});
							return FReply::Handled();
						})
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CloseThread", "Close"))
						.OnClicked_Lambda([] { FStudioQuickAsk::Close(); return FReply::Handled(); })
					]
				];
		}

		FPillTheme Theme;
		TArray<FSlateRoundedBoxBrush> PillBrushes;
		FSlateRoundedBoxBrush ChipBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent);
		FSlateRoundedBoxBrush BoxNormal = FSlateRoundedBoxBrush(FLinearColor::Transparent);
		FSlateRoundedBoxBrush BoxFocused = FSlateRoundedBoxBrush(FLinearColor::Transparent);
		FSlateRoundedBoxBrush SendNormal = FSlateRoundedBoxBrush(FLinearColor::Transparent);
		FSlateRoundedBoxBrush SendHovered = FSlateRoundedBoxBrush(FLinearColor::Transparent);
		FEditableTextBoxStyle BoxStyle;
		FButtonStyle SendStyle;

		TSharedRef<SWidget> Chip(const FText& Text, const FText& Tooltip)
		{
			return SNew(SBorder)
				.BorderImage(&ChipBrush)
				.Padding(FMargin(8.0f, 2.0f))
				.ToolTipText(Tooltip)
				[
					SNew(STextBlock).Font(FAppStyle::GetFontStyle("SmallFont")).Text(Text)
						.ColorAndOpacity(Theme.bEditor ? FSlateColor::UseForeground() : FSlateColor(Theme.Text))
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

	/**
	 * The asset a graph edits. Some editors (materials) edit a transient copy, so
	 * that copy is matched by name against the assets open in an editor.
	 */
	FString GraphAsset(const UEdGraph* Graph)
	{
		const UPackage* Package = Graph->GetOutermost();
		if (Package->GetName() != TEXT("/Engine/Transient"))
		{
			return Package->GetName();
		}
		const UObject* Top = Graph;
		while (Top->GetOuter() && Top->GetOuter() != Package)
		{
			Top = Top->GetOuter();
		}
		if (GEditor)
		{
			if (UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				for (const UObject* Asset : Editors->GetAllEditedAssets())
				{
					if (Asset && Asset->GetFName() == Top->GetFName() && Asset->GetOutermost() != Package)
					{
						return Asset->GetPathName();
					}
				}
			}
		}
		return Package->GetName();
	}

	FStudioQuickContext NodeContext(const UEdGraphNode* Node, const FString& Asset, bool bHovered)
	{
		FStudioQuickContext C;
		C.Kind = TEXT("node");
		C.Label = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		C.Class = Node->GetClass()->GetName();
		// The node's own path, unless it lives in a transient copy that nothing else can find.
		C.Path = Node->GetOutermost()->GetName() == TEXT("/Engine/Transient")
			? FString::Printf(TEXT("%s, graph %s, node %s"), *Asset, *Node->GetGraph()->GetName(), *Node->GetName())
			: Node->GetPathName();
		C.At = bHovered ? TEXT("under the cursor") : TEXT("selected");
		return C;
	}

	/**
	 * Over a graph editor (Blueprint, material, Niagara and the rest share it): the
	 * graph with the cursor's position in it, the node under the cursor, then the
	 * other selected nodes. Empty anywhere else.
	 */
	TArray<FStudioQuickContext> GraphContext(FSlateApplication& App, const FVector2D& Screen)
	{
		TArray<FStudioQuickContext> Out;
		const FWidgetPath Path = App.LocateWindowUnderMouse(Screen, App.GetInteractiveTopLevelWindows());
		static const FName GraphPanel(TEXT("SGraphPanel"));
		for (int32 i = Path.Widgets.Num() - 1; i >= 0; --i)
		{
			if (Path.Widgets[i].Widget->GetType() != GraphPanel)
			{
				continue;
			}
			const TSharedRef<SGraphPanel> Panel = StaticCastSharedRef<SGraphPanel>(Path.Widgets[i].Widget);
			const UEdGraph* Graph = Panel->GetGraphObj();
			if (!Graph)
			{
				return Out;
			}
			FStudioQuickContext G;
			G.Kind = TEXT("graph");
			G.Label = Graph->GetName();
			G.Class = GraphAsset(Graph);
			G.Path = Graph->GetOutermost()->GetName() == TEXT("/Engine/Transient") ? FString::Printf(TEXT("%s, graph %s"), *G.Class, *Graph->GetName()) : Graph->GetPathName();
			const FVector2D Local = Path.Widgets[i].Geometry.AbsoluteToLocal(Screen);
			const FVector2D InGraph = Panel->PanelCoordToGraphCoord(Local);
			G.At = FString::Printf(TEXT("X=%.0f Y=%.0f"), InGraph.X, InGraph.Y);
			Out.Add(G);
			// A graph panel's children are its nodes, so the next widget down the path is the node under the cursor.
			const UEdGraphNode* Hovered = nullptr;
			// Its node widgets are all SGraphNode subclasses (SGraphNodeK2Default, SNiagaraGraphNode, ...); the name check guards the cast.
			if (i + 1 < Path.Widgets.Num() && Path.Widgets[i + 1].Widget->GetType().ToString().Contains(TEXT("Node")))
			{
				Hovered = StaticCastSharedRef<SGraphNode>(Path.Widgets[i + 1].Widget)->GetNodeObj();
				if (Hovered)
				{
					Out.Add(NodeContext(Hovered, G.Class, true));
				}
			}
			for (const UEdGraphNode* Node : Panel->GetSelectedGraphNodes())
			{
				if (Node && Node != Hovered)
				{
					Out.Add(NodeContext(Node, G.Class, false));
				}
			}
			return Out;
		}
		return Out;
	}

	/** What a hotkey or gesture carries: over the level viewport the point under the cursor (and with the gesture the selected actors), over a graph its nodes. */
	TArray<FStudioQuickContext> ViewportContext(FSlateApplication& App, const FVector2D& Screen, bool bSelection)
	{
		TArray<FStudioQuickContext> Out = GraphContext(App, Screen);
		if (Out.Num())
		{
			return Out;
		}
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
	ConsoleCommands.Add(Console.RegisterConsoleCommand(TEXT("Studio.QuickAsk.ContextAt"),
		TEXT("Log the context the pill would carry from a screen point, in pixels, without opening it. Studio.QuickAsk.ContextAt X Y"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2 || !FSlateApplication::IsInitialized())
			{
				return;
			}
			const FVector2D At(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
			UE_LOG(LogStudioQuickAsk, Log, TEXT("context at %.0f,%.0f: %s"), At.X, At.Y,
				*FStudioQuickAsk::RequestBody(TEXT("-"), ViewportContext(FSlateApplication::Get(), At, true)));
		})));
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
	/**
	 * Top-left corner just below and right of the cursor, like a tooltip; flipped
	 * to the cursor's other side where the monitor's work area runs out. Screen pixels.
	 */
	FVector2D PlaceAtCursor(const FVector2D& Size)
	{
		FSlateApplication& App = FSlateApplication::Get();
		const FVector2D Cursor = App.GetCursorPos();
		const float Dpi = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(Cursor.X, Cursor.Y);
		const FSlateRect Work = App.GetWorkArea(FSlateRect(Cursor, Cursor));
		const float Margin = 8.0f * Dpi;
		const FVector2D Offset(12.0 * Dpi, 16.0 * Dpi);
		FVector2D Pos = Cursor + Offset;
		const bool bLeft = Pos.X + Size.X > Work.Right - Margin;
		const bool bAbove = Pos.Y + Size.Y > Work.Bottom - Margin;
		if (bLeft)
		{
			Pos.X = Cursor.X - Offset.X - Size.X;
		}
		if (bAbove)
		{
			Pos.Y = Cursor.Y - Offset.Y - Size.Y;
		}
		PointCorner = bAbove ? (bLeft ? 2 : 3) : (bLeft ? 1 : 0);
		Pos.X = FMath::Clamp(Pos.X, Work.Left + Margin, FMath::Max(Work.Left + Margin, Work.Right - Margin - Size.X));
		Pos.Y = FMath::Clamp(Pos.Y, Work.Top + Margin, FMath::Max(Work.Top + Margin, Work.Bottom - Margin - Size.Y));
		return Pos;
	}

	/**
	 * Clip the native window to the pill's rounded outline. Per-pixel transparency
	 * is not reliable across RHIs and draws the corners black; a window region
	 * leaves nothing there to draw. Kept in step with the window's size and the corner it points from.
	 */
	void TrackShape()
	{
#if PLATFORM_WINDOWS
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Shaped = FIntPoint(0, 0), ShapedCorner = -1](float) mutable
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
			if ((Size != Shaped || PointCorner != ShapedCorner) && Size.X > 0 && Size.Y > 0)
			{
				Shaped = Size;
				ShapedCorner = PointCorner;
				const int32 D = FMath::RoundToInt(2.0f * PillRadius * Native->GetDPIScaleFactor());
				// Rounded everywhere, then the corner that points at the subject filled back in square.
				HRGN Shape = ::CreateRoundRectRgn(0, 0, Size.X + 1, Size.Y + 1, D, D);
				const int32 Half = D / 2 + 1;
				const int32 Corner = PointCorner & 3;
				const int32 X = Corner == 1 || Corner == 2 ? Size.X - Half : 0;
				const int32 Y = Corner >= 2 ? Size.Y - Half : 0;
				HRGN Square = ::CreateRectRgn(X, Y, X + Half, Y + Half);
				::CombineRgn(Shape, Shape, Square, RGN_OR);
				::DeleteObject(Square);
				::SetWindowRgn(Hwnd, Shape, 1);
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
		// Slate centres a new window on the work area unless told not to, whatever ScreenPosition says.
		.AutoCenter(EAutoCenter::None)
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
		if (TSharedPtr<SPill> P = Pill.Pin(); P.IsValid() && P->IsThread())
		{
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
	Window->MoveWindowTo(Pos);
	Window->BringToFront(true);
	Widget->FocusBox();
	TrackShape();
	UE_LOG(LogStudioQuickAsk, Log, TEXT("pill at %.0f,%.0f for cursor %.0f,%.0f"), Pos.X, Pos.Y, Cursor.X, Cursor.Y);
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
			if (!C.At.IsEmpty())
			{
				Item->SetStringField(TEXT("at"), C.At);
			}
			Items.Add(MakeShared<FJsonValueObject>(Item));
		}
		Body->SetArrayField(TEXT("context"), Items);
	}
	FString Out;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out));
	return Out;
}

FString FStudioQuickAsk::TablesAsRows(const FString& Markup)
{
	// A row's cells: outer pipes dropped, split on pipes not escaped as \|.
	auto Cells = [](const FString& Line)
	{
		FString Row = Line.TrimStartAndEnd();
		Row.RemoveFromStart(TEXT("|"));
		if (Row.EndsWith(TEXT("|")) && !Row.EndsWith(TEXT("\\|")))
		{
			Row.LeftChopInline(1);
		}
		TArray<FString> Out;
		FString Cell;
		for (int32 i = 0; i < Row.Len(); ++i)
		{
			if (Row[i] == TEXT('\\') && i + 1 < Row.Len() && Row[i + 1] == TEXT('|'))
			{
				Cell.AppendChar(TEXT('|'));
				++i;
			}
			else if (Row[i] == TEXT('|'))
			{
				Out.Add(Cell.TrimStartAndEnd());
				Cell.Reset();
			}
			else
			{
				Cell.AppendChar(Row[i]);
			}
		}
		Out.Add(Cell.TrimStartAndEnd());
		return Out;
	};
	auto IsSeparator = [&Cells](const FString& Line)
	{
		if (!Line.Contains(TEXT("-")) || !Line.Contains(TEXT("|")))
		{
			return false;
		}
		for (const FString& Cell : Cells(Line))
		{
			FString Dashes = Cell.Replace(TEXT(":"), TEXT(""));
			if (Dashes.IsEmpty() || Dashes.Replace(TEXT("-"), TEXT("")).Len() > 0)
			{
				return false;
			}
		}
		return true;
	};

	TArray<FString> Lines;
	Markup.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Lines, TEXT("\n"), false);
	TArray<FString> Out;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		// A table is a header row with pipes and a separator row under it.
		if (!Lines[i].Contains(TEXT("|")) || i + 1 >= Lines.Num() || !IsSeparator(Lines[i + 1]))
		{
			Out.Add(Lines[i]);
			continue;
		}
		const TArray<FString> Header = Cells(Lines[i]);
		for (i += 2; i < Lines.Num() && Lines[i].Contains(TEXT("|")); ++i)
		{
			if (IsSeparator(Lines[i]))
			{
				continue;
			}
			const TArray<FString> Row = Cells(Lines[i]);
			bool bFirst = true;
			for (int32 c = 0; c < Row.Num(); ++c)
			{
				if (Row[c].IsEmpty())
				{
					continue;
				}
				// A header that carries markup of its own is left as it is: rich text tags do not nest.
				const FString Name = Header.IsValidIndex(c) ? Header[c] : FString();
				const FString Label = Name.IsEmpty() ? FString() : Name.Contains(TEXT("<")) ? Name + TEXT(": ") : TEXT("<b>") + Name + TEXT("</>: ");
				Out.Add((bFirst ? FString(TEXT("•  ")) : FString(TEXT("    "))) + Label + Row[c]);
				bFirst = false;
			}
		}
		--i;
	}
	return FString::Join(Out, TEXT("\n"));
}

FString FStudioQuickAsk::StepLabel(const FString& Name, const FJsonObject& Input)
{
	auto Str = [&Input](const TCHAR* Key)
	{
		FString V;
		Input.TryGetStringField(Key, V);
		return V;
	};
	auto Num = [&Input](const TCHAR* Key)
	{
		double V = 0.0;
		return Input.TryGetNumberField(Key, V) ? static_cast<int64>(V) : int64(0);
	};
	const FString Path = Str(TEXT("path"));
	const FString In = Path.IsEmpty() ? FString() : TEXT(" in ") + Path;
	if (Name == TEXT("editor_restart"))
	{
		return TEXT("editor ") + Str(TEXT("operation"));
	}
	if (Name == TEXT("shell"))
	{
		TArray<FString> Lines;
		Str(TEXT("command")).TrimStartAndEnd().Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Lines, TEXT("\n"), false);
		return Lines.Num() > 1 ? FString::Printf(TEXT("%s (+%d lines)"), *Lines[0], Lines.Num() - 1) : Lines.Num() ? Lines[0] : FString();
	}
	if (Name == TEXT("read_file"))
	{
		return Num(TEXT("offset")) || Num(TEXT("limit")) ? FString::Printf(TEXT("%s (from line %lld)"), *Path, Num(TEXT("offset")) ? Num(TEXT("offset")) : int64(1)) : Path;
	}
	if (Name == TEXT("write_file") || Name == TEXT("edit_file"))
	{
		return Path;
	}
	if (Name == TEXT("glob"))
	{
		return Str(TEXT("pattern")) + In;
	}
	if (Name == TEXT("grep"))
	{
		const FString Glob = Str(TEXT("glob"));
		return TEXT("/") + Str(TEXT("pattern")) + TEXT("/") + (Glob.IsEmpty() ? FString() : TEXT(" ") + Glob) + In;
	}
	if (Name == TEXT("web_fetch"))
	{
		return Str(TEXT("url"));
	}
	if (Name == TEXT("web_search"))
	{
		return TEXT("\"") + Str(TEXT("query")) + TEXT("\"");
	}
	// The ue-mcp gateway: an action, then what it names.
	const FString Action = Str(TEXT("action"));
	if (Action.IsEmpty())
	{
		return FString();
	}
	const FString Category = Str(TEXT("category"));
	const FString Method = Str(TEXT("method"));
	if (Action == TEXT("call"))
	{
		return Category + TEXT(".") + Method;
	}
	if (Action == TEXT("search"))
	{
		return TEXT("search \"") + Str(TEXT("query")) + TEXT("\"");
	}
	return TEXT("describe ") + Category + (Method.IsEmpty() ? FString() : TEXT(".") + Method);
}

TArray<FString> FStudioQuickAsk::MainStepLabels(const FString& SessionBody)
{
	TArray<FString> Labels;
	TSharedPtr<FJsonObject> Session;
	const TArray<TSharedPtr<FJsonValue>>* Transcript = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SessionBody), Session) || !Session.IsValid() || !Session->TryGetArrayField(TEXT("transcript"), Transcript))
	{
		return Labels;
	}
	for (const TSharedPtr<FJsonValue>& V : *Transcript)
	{
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		FString Kind, Parent;
		// A subagent's entries name the agent call that ran them; the pill counts the main thread's only.
		if (!V.IsValid() || !V->TryGetObject(Entry) || !(*Entry)->TryGetStringField(TEXT("kind"), Kind) || Kind != TEXT("tool")
			|| ((*Entry)->TryGetStringField(TEXT("parent"), Parent) && !Parent.IsEmpty()))
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* Call = nullptr;
		const TSharedPtr<FJsonObject>* Input = nullptr;
		FString Name;
		const bool bCall = (*Entry)->TryGetObjectField(TEXT("call"), Call) && (*Call)->TryGetStringField(TEXT("name"), Name);
		Labels.Add(bCall && (*Call)->TryGetObjectField(TEXT("input"), Input) ? StepLabel(Name, **Input) : FString());
	}
	return Labels;
}

#undef LOCTEXT_NAMESPACE
