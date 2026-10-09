#include "SStudioPanel.h"

#include "StudioDaemon.h"
#include "StudioNativeBinding.h"
#include "Containers/Ticker.h"
#include "IWebBrowserWindow.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"
#include "WebBrowserModule.h"
#include "IWebBrowserSingleton.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogStudioPanel, Log, All);

void SStudioPanel::Construct(const FArguments& InArgs)
{
	Binding.Reset(NewObject<UStudioNativeBinding>());

	// Get() loads the module; IsAvailable() before it fails on a module not yet loaded.
	IWebBrowserModule& WebModule = IWebBrowserModule::Get();
	if (!IWebBrowserModule::IsAvailable() || !WebModule.IsWebModuleAvailable())
	{
		UE_LOG(LogStudioPanel, Error, TEXT("the web browser module is not available"));
		ChildSlot[SNew(STextBlock).Text(NSLOCTEXT("UE_MCP_Studio", "NoBrowser", "The editor's web browser is not available, so Studio cannot be shown here. Open it in a browser with: npx ue-mcp daemon open"))];
		return;
	}

	// Created explicitly rather than by SWebBrowser, so a failure is reported instead of a spinner that never ends.
	FCreateBrowserWindowSettings Settings;
	Settings.InitialURL = BootstrapUrl();
	Settings.BrowserFrameRate = 60;
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
	Settings.UserAgentApplication = IWebBrowserModule::MakeUserAgentApplication(TEXT("UE-MCP-Studio"));
#endif
	IWebBrowserSingleton* Singleton = WebModule.GetSingleton();
	const TSharedPtr<IWebBrowserWindow> Window = Singleton ? Singleton->CreateBrowserWindow(Settings) : nullptr;
	if (!Window.IsValid())
	{
		UE_LOG(LogStudioPanel, Error, TEXT("could not create a browser window (singleton %s)"), Singleton ? TEXT("present") : TEXT("missing"));
		ChildSlot[SNew(STextBlock).Text(NSLOCTEXT("UE_MCP_Studio", "NoWindow", "The editor could not create a browser view for Studio. Open it in a browser with: npx ue-mcp daemon open"))];
		return;
	}
	UE_LOG(LogStudioPanel, Log, TEXT("browser window created"));

	ChildSlot
	[
		SAssignNew(Browser, SWebBrowser, Window)
		.ShowControls(false)
		.ShowAddressBar(false)
		.OnBeforeNavigation(this, &SStudioPanel::OnBeforeNavigation)
		.OnBeforePopup(this, &SStudioPanel::OnBeforePopup)
	];
	Browser->BindUObject(TEXT("studio"), Binding.Get(), true);

	Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SStudioPanel::Poll), 1.0f);
	Poll(0.f);
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
	// A bridge installed as binaries from the bootstrap page loads without a restart.
	if (!FStudioDaemon::IsBridgeInstalled() && FStudioDaemon::BridgeMarkerKind() == TEXT("binary") && !bTriedLoadBridge)
	{
		bTriedLoadBridge = true;
		FString Error;
		if (FStudioDaemon::LoadInstalledBridge(Error))
		{
			FStudioDaemon::EnsureRunning();
		}
		else
		{
			UE_LOG(LogStudioPanel, Warning, TEXT("%s"), *Error);
		}
	}

	FStudioDaemonInfo Info;
	const bool bLive = FStudioDaemon::ReadLive(Info);
	if (bLive && (Info.Port != LoadedPort || Info.Token != LoadedToken))
	{
		// A daemon came up, or a new one replaced the one the page was talking to.
		LoadedPort = Info.Port;
		LoadedToken = Info.Token;
		UE_LOG(LogStudioPanel, Log, TEXT("loading the UI from the daemon on port %d"), Info.Port);
		Browser->LoadURL(FStudioDaemon::UiUrl(Info));
	}
	else if (!bLive && LoadedPort != 0)
	{
		// The page reconnects on its own if the daemon returns on the same port;
		// a different port is caught above. Until then the bootstrap explains.
		LoadedPort = 0;
		LoadedToken.Reset();
		Browser->LoadURL(BootstrapUrl());
	}
	return true;
}

bool SStudioPanel::IsAllowed(const FString& Url) const
{
	if (Url.StartsWith(BootstrapUrl().Left(BootstrapUrl().Len() - FString(TEXT("bootstrap.html")).Len())))
	{
		return true;
	}
	return LoadedPort != 0 && Url.StartsWith(FString::Printf(TEXT("http://127.0.0.1:%d/"), LoadedPort));
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
	UE_LOG(LogStudioPanel, Warning, TEXT("blocked popup to %s"), *Url);
	return true;
}
