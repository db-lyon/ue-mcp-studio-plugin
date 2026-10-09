#include "SStudioPanel.h"

#include "StudioDaemon.h"
#include "StudioNativeBinding.h"
#include "Containers/Ticker.h"
#include "IWebBrowserWindow.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "SWebBrowser.h"

DEFINE_LOG_CATEGORY_STATIC(LogStudioPanel, Log, All);

void SStudioPanel::Construct(const FArguments& InArgs)
{
	Binding.Reset(NewObject<UStudioNativeBinding>());

	ChildSlot
	[
		SAssignNew(Browser, SWebBrowser)
		.InitialURL(BootstrapUrl())
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowErrorMessage(true)
		.BackgroundColor(FColor(26, 26, 26))
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
