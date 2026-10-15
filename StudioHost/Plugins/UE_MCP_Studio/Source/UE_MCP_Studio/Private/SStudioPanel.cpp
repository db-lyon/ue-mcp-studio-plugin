// Copyright 2026 ue-mcp. All Rights Reserved.

#include "SStudioPanel.h"

#include "SStudioKeyGuard.h"
#include "StudioConsent.h"
#include "StudioDaemon.h"
#include "StudioDock.h"
#include "StudioNativeBinding.h"
#include "StudioWindow.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "IWebBrowserWindow.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "SWebBrowser.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioPanel, Log, All);

namespace
{
	constexpr double HealthEverySeconds = 2.0;
	constexpr double UpdatesEverySeconds = 600.0;

	/** The switcher's pages. */
	constexpr int32 SetupPage = 0;
	constexpr int32 LauncherPage = 1;
	constexpr int32 ConsentPage = 2;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> DaemonGet(const FStudioDaemonInfo& Info, const FString& Path)
	{
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d%s"), Info.Port, *Path));
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Info.Token);
		return Request;
	}

	TSharedPtr<FJsonObject> JsonBody(FHttpResponsePtr Response, bool bOk)
	{
		TSharedPtr<FJsonObject> Json;
		if (bOk && Response.IsValid() && Response->GetResponseCode() == 200)
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Json);
		}
		return Json;
	}
}

void SStudioPanel::Construct(const FArguments& InArgs)
{
	Binding.Reset(NewObject<UStudioNativeBinding>());
	DaemonText = LOCTEXT("DaemonStarting", "Daemon: starting");

	TSharedRef<SWidget> Setup = SNullWidget::NullWidget;
	// Get() loads the module; IsAvailable() before it fails on a module not yet loaded.
	IWebBrowserModule& WebModule = IWebBrowserModule::Get();
	IWebBrowserSingleton* Singleton = IWebBrowserModule::IsAvailable() && WebModule.IsWebModuleAvailable() ? WebModule.GetSingleton() : nullptr;
	FCreateBrowserWindowSettings Settings;
	Settings.InitialURL = BootstrapUrl();
	Settings.BrowserFrameRate = 60;
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
	Settings.UserAgentApplication = IWebBrowserModule::MakeUserAgentApplication(TEXT("UE-MCP-Studio"));
#endif
	// Created explicitly rather than by SWebBrowser, so a failure is reported instead of a spinner that never ends.
	const TSharedPtr<IWebBrowserWindow> Window = Singleton ? Singleton->CreateBrowserWindow(Settings) : nullptr;
	if (Window.IsValid())
	{
		Setup = SNew(SStudioKeyGuard)
		[
			SAssignNew(Browser, SWebBrowser, Window)
			.ShowControls(false)
			.ShowAddressBar(false)
			.OnBeforeNavigation(this, &SStudioPanel::OnBeforeNavigation)
			.OnBeforePopup(this, &SStudioPanel::OnBeforePopup)
		];
		Browser->BindUObject(TEXT("studio"), Binding.Get(), true);
	}
	else
	{
		UE_LOG(LogStudioPanel, Error, TEXT("could not create a browser view for the setup page (singleton %s)"), Singleton ? TEXT("present") : TEXT("missing"));
		Setup = SNew(SBox).Padding(16.f)
		[
			SNew(STextBlock).AutoWrapText(true)
			.Text(LOCTEXT("NoBrowser", "The editor's web browser is not available, so the setup page cannot be shown. Run npx ue-mcp init in the project folder, then open Studio again."))
		];
	}

	ChildSlot
	[
		SAssignNew(Switcher, SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()[Setup]
		+ SWidgetSwitcher::Slot()[MakeLauncher()]
		+ SWidgetSwitcher::Slot()[MakeConsent()]
	];

	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SStudioPanel::Poll), 1.0f);
	Poll(0.f);
}

TSharedRef<SWidget> SStudioPanel::MakeLauncher()
{
	auto Line = [](TAttribute<FText> Text)
	{
		return SNew(STextBlock).Text(Text).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground());
	};
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(20.f, 18.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Text(LOCTEXT("LauncherTitle", "UE-MCP Studio")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(STextBlock).AutoWrapText(true)
				.Text(LOCTEXT("LauncherAbout", "Studio runs in a process of its own, docked in an editor tab or in its own window, so the chat keeps working while the editor is busy."))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 16.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.ContentPadding(FMargin(14.f, 5.f))
					.OnClicked_Lambda([]() { FStudioDock::OpenStudio(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text_Lambda([]() { return FStudioWindow::IsOpen() ? LOCTEXT("ShowStudio", "Show Studio") : LOCTEXT("OpenStudio", "Open Studio"); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(14.f, 5.f))
					.OnClicked_Lambda([]() { FStudioDock::SetDocked(!FStudioDock::IsDockedMode()); return FReply::Handled(); })
					[
						SNew(STextBlock).Text_Lambda([]() { return FStudioDock::IsDockedMode() ? LOCTEXT("OpenAsWindow", "Open as window") : LOCTEXT("DockInEditor", "Dock in editor"); })
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				Line(TAttribute<FText>::CreateLambda([]()
				{
					if (!FStudioWindow::IsOpen())
					{
						return LOCTEXT("WindowClosed", "Window: not open");
					}
					return FStudioDock::IsDockedMode() ? LOCTEXT("WindowDocked", "Window: open, docked in the UE-MCP Studio tab") : LOCTEXT("WindowOpen", "Window: open");
				}))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				Line(TAttribute<FText>::CreateLambda([this]() { return EditorText; }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				Line(TAttribute<FText>::CreateLambda([this]() { return DaemonText; }))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				Line(TAttribute<FText>::CreateLambda([this]() { return UpdateText; }))
			]
		];
}

TSharedRef<SWidget> SStudioPanel::MakeConsent()
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	for (const FStudioInstallTarget& Target : FStudioConsent::Targets())
	{
		List->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).AutoWrapText(true).Text(Target.What)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(12.f, 2.f, 0.f, 0.f)
			[
				SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9)).Text(FText::FromString(Target.Where))
			]
		];
	}
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(20.f, 18.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 6.f)
			[
				SNew(STextBlock).Text(LOCTEXT("ConsentTitle", "Allow Studio to install its parts?")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
			[
				SNew(STextBlock).AutoWrapText(true)
				.Text(LOCTEXT("ConsentAbout", "Studio needs the parts below. They are downloaded from npm (registry.npmjs.org) and GitHub (github.com) and installed here. Nothing is downloaded or installed until you allow it. You are asked once."))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
			[
				List
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
			[
				SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text(FText::Format(LOCTEXT("ConsentRecord", "Your answer is kept in {0}. Node.js is not installed by Studio."), FText::FromString(FPaths::ConvertRelativePathToFull(FStudioConsent::FilePath()))))
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
					.ContentPadding(FMargin(14.f, 5.f))
					.OnClicked_Lambda([this]()
					{
						FStudioConsent::Grant();
						Switcher->SetActiveWidgetIndex(SetupPage);
						// The page loaded before consent, when its calls were refused.
						if (Browser.IsValid())
						{
							Browser->LoadURL(BootstrapUrl());
						}
						// Studio's open was refused before consent; this starts the daemon and opens Studio once it is up.
						FStudioDock::OpenAfterSetup();
						return FReply::Handled();
					})
					[
						SNew(STextBlock).Text(LOCTEXT("ConsentAllow", "Allow"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(FMargin(14.f, 5.f))
					// Nothing is kept, so the next open asks again.
					.OnClicked_Lambda([]() { FStudioDock::CloseTab(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("ConsentNotNow", "Not now"))
					]
				]
			]
		];
}

SStudioPanel::~SStudioPanel()
{
	FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
}

FString SStudioPanel::BootstrapUrl() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE_MCP_Studio"));
	FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"), TEXT("web"), TEXT("bootstrap.html")));
	File.ReplaceInline(TEXT("\\"), TEXT("/"));
	return FString(TEXT("file:///")) + File;
}

bool SStudioPanel::Poll(float)
{
	// A bridge installed as binaries from the setup page loads without a restart.
	if (!FStudioDaemon::IsBridgeInstalled() && FStudioDaemon::BridgeMarkerKind() == TEXT("binary") && !bTriedLoadBridge)
	{
		bTriedLoadBridge = true;
		FString Error;
		if (FStudioDaemon::LoadInstalledBridge(Error))
		{
			FStudioDock::OpenAfterSetup();
		}
		else
		{
			UE_LOG(LogStudioPanel, Warning, TEXT("%s"), *Error);
		}
	}

	// A daemon another MCP client started is not Studio: without consent the prompt shows regardless.
	FStudioDaemonInfo Info;
	const bool bLive = FStudioDaemon::ReadReady(Info);
	if (!bLive && !FStudioConsent::IsGranted())
	{
		LivePort = 0;
		Switcher->SetActiveWidgetIndex(ConsentPage);
		return true;
	}
	if (bLive)
	{
		if (Info.Port != LivePort)
		{
			// A daemon came up, or a new one replaced the one shown.
			LivePort = Info.Port;
			LastHealth = LastUpdates = 0.0;
			Switcher->SetActiveWidgetIndex(LauncherPage);
		}
		DaemonText = FText::Format(LOCTEXT("DaemonLive", "Daemon: ue-mcp {0} on port {1}"), FText::FromString(Info.Version), FText::AsNumber(Info.Port, &FNumberFormattingOptions::DefaultNoGrouping()));
		const double Now = FPlatformTime::Seconds();
		if (Now - LastHealth >= HealthEverySeconds)
		{
			LastHealth = Now;
			RequestHealth(Info);
		}
		if (Now - LastUpdates >= UpdatesEverySeconds)
		{
			LastUpdates = Now;
			RequestUpdates(Info);
		}
		FStudioWindow::PublishEditorTheme();
	}
	else if (LivePort != 0 || Switcher->GetActiveWidgetIndex() != SetupPage)
	{
		// The setup page explains and offers to start it again.
		LivePort = 0;
		Switcher->SetActiveWidgetIndex(SetupPage);
		if (Browser.IsValid())
		{
			Browser->LoadURL(BootstrapUrl());
		}
	}
	return true;
}

void SStudioPanel::RequestHealth(const FStudioDaemonInfo& Info)
{
	TWeakPtr<SStudioPanel> Weak = StaticCastSharedRef<SStudioPanel>(AsShared());
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = DaemonGet(Info, TEXT("/v1/health"));
	Request->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		const TSharedPtr<SStudioPanel> Self = Weak.Pin();
		const TSharedPtr<FJsonObject> Json = JsonBody(Response, bOk);
		if (!Self.IsValid() || !Json.IsValid())
		{
			return;
		}
		bool bConnected = false;
		const TArray<TSharedPtr<FJsonValue>>* Editors = nullptr;
		if (Json->TryGetArrayField(TEXT("editors"), Editors) && Editors->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* Editor = nullptr;
			if ((*Editors)[0]->TryGetObject(Editor))
			{
				(*Editor)->TryGetBoolField(TEXT("connected"), bConnected);
			}
		}
		Self->EditorText = bConnected
			? LOCTEXT("EditorConnected", "Editor: connected to Studio")
			: LOCTEXT("EditorNotConnected", "Editor: not connected to Studio yet");
	});
	Request->ProcessRequest();
}

void SStudioPanel::RequestUpdates(const FStudioDaemonInfo& Info)
{
	TWeakPtr<SStudioPanel> Weak = StaticCastSharedRef<SStudioPanel>(AsShared());
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = DaemonGet(Info, TEXT("/v1/ext/studio/updates"));
	Request->OnProcessRequestComplete().BindLambda([Weak](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		const TSharedPtr<SStudioPanel> Self = Weak.Pin();
		if (!Self.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject> Json = JsonBody(Response, bOk);
		const TSharedPtr<FJsonObject>* Studio = nullptr;
		const TSharedPtr<FJsonObject>* Plugin = nullptr;
		if (!Json.IsValid() || !Json->TryGetObjectField(TEXT("studio"), Studio) || !Json->TryGetObjectField(TEXT("plugin"), Plugin))
		{
			Self->UpdateText = LOCTEXT("UpdatesUnknown", "Updates: could not check");
			return;
		}
		FString Error;
		if (Json->TryGetStringField(TEXT("error"), Error) && !Error.IsEmpty())
		{
			Self->UpdateText = FText::Format(LOCTEXT("UpdatesError", "Updates: could not check ({0})"), FText::FromString(Error));
			return;
		}
		TArray<FString> Ready;
		FString Installed, PluginInstalled, Latest;
		(*Studio)->TryGetStringField(TEXT("installed"), Installed);
		(*Plugin)->TryGetStringField(TEXT("installed"), PluginInstalled);
		bool bAvailable = false;
		if ((*Studio)->TryGetBoolField(TEXT("available"), bAvailable) && bAvailable && (*Studio)->TryGetStringField(TEXT("latest"), Latest))
		{
			Ready.Add(FString::Printf(TEXT("Studio %s"), *Latest));
		}
		bAvailable = false;
		const bool bPluginUpdate = (*Plugin)->TryGetBoolField(TEXT("available"), bAvailable) && bAvailable && (*Plugin)->TryGetStringField(TEXT("latest"), Latest);
#if UE_MCP_STUDIO_FAB
		// Fab owns this copy's files, so its updates come from Fab.
		if (bPluginUpdate && Ready.Num() == 0)
		{
			Self->UpdateText = FText::Format(LOCTEXT("UpdatesFab", "Updates: the editor plugin {0} is out. Update it from Fab."), FText::FromString(Latest));
			return;
		}
#else
		if (bPluginUpdate)
		{
			Ready.Add(FString::Printf(TEXT("the editor plugin %s"), *Latest));
		}
#endif
		Self->UpdateText = Ready.Num() > 0
			? FText::Format(LOCTEXT("UpdatesReady", "Updates: {0} ready. Install from Studio's Settings > Update."), FText::FromString(FString::Join(Ready, TEXT(" and "))))
			: FText::Format(LOCTEXT("UpdatesCurrent", "Updates: Studio {0} and the editor plugin {1} are current"), FText::FromString(Installed), FText::FromString(PluginInstalled.IsEmpty() ? TEXT("(engine)") : PluginInstalled));
	});
	Request->ProcessRequest();
}

bool SStudioPanel::IsAllowed(const FString& Url) const
{
	const FString Bootstrap = BootstrapUrl();
	return Url.StartsWith(Bootstrap.Left(Bootstrap.Len() - FString(TEXT("bootstrap.html")).Len()));
}

bool SStudioPanel::OnBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request)
{
	if (IsAllowed(Url))
	{
		return false;
	}
	UE_LOG(LogStudioPanel, Warning, TEXT("blocked navigation to %s"), *Url);
	return true;
}

bool SStudioPanel::OnBeforePopup(FString Url, FString Frame)
{
	// Links the page opens in a new window go to the system browser; nothing else leaves the tab.
	if (Url.StartsWith(TEXT("https://")) || Url.StartsWith(TEXT("http://")))
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	}
	else
	{
		UE_LOG(LogStudioPanel, Warning, TEXT("blocked popup to %s"), *Url);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
