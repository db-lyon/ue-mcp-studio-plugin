// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioDocs.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/PlatformProcess.h"
#include "SStudioKeyGuard.h"
#include "SWebBrowser.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioDocs, Log, All);

namespace
{
	const FName DocsTab(TEXT("UEMCPDocs"));
	const FString Site(TEXT("https://ue-mcp.com"));
	const FString DefaultPath(TEXT("/docs/"));

	/** Where the next spawned tab starts, and the open tab's browser. */
	FString PendingUrl;
	TWeakPtr<SWebBrowser> OpenBrowser;

	bool OnSite(const FString& Url)
	{
		return Url == Site || Url.StartsWith(Site + TEXT("/")) || Url.StartsWith(Site + TEXT("?")) || Url.StartsWith(Site + TEXT("#"));
	}

	bool OnBeforeNavigation(const FString& Url, const FWebNavigationRequest&)
	{
		if (OnSite(Url))
		{
			return false;
		}
		// Off the site: the system browser, not this tab.
		if (Url.StartsWith(TEXT("https://")) || Url.StartsWith(TEXT("http://")))
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		}
		return true;
	}

	bool OnBeforePopup(FString Url, FString)
	{
		if (OnSite(Url))
		{
			if (const TSharedPtr<SWebBrowser> Browser = OpenBrowser.Pin())
			{
				Browser->LoadURL(Url);
			}
		}
		else if (Url.StartsWith(TEXT("https://")) || Url.StartsWith(TEXT("http://")))
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		}
		return true;
	}

	TSharedRef<SDockTab> SpawnDocsTab(const FSpawnTabArgs&)
	{
		const FString Url = PendingUrl.IsEmpty() ? Site + DefaultPath : PendingUrl;
		PendingUrl.Reset();
		TSharedRef<SWebBrowser> Browser = SNew(SWebBrowser)
			.InitialURL(Url)
			.ShowControls(true)
			.ShowAddressBar(false)
			.OnBeforeNavigation_Static(&OnBeforeNavigation)
			.OnBeforePopup_Static(&OnBeforePopup);
		OpenBrowser = Browser;
		return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SStudioKeyGuard)[Browser]];
	}
}

void FStudioDocs::Register()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DocsTab, FOnSpawnTab::CreateStatic(&SpawnDocsTab))
		.SetDisplayName(LOCTEXT("DocsTabTitle", "UE-MCP Docs"))
		.SetTooltipText(LOCTEXT("DocsTabTooltip", "The ue-mcp documentation"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Help"));
}

void FStudioDocs::Unregister()
{
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DocsTab);
	}
}

bool FStudioDocs::Open(const FString& Path)
{
	const FString Wanted = Path.IsEmpty() ? DefaultPath : Path;
	if (!Wanted.StartsWith(TEXT("/docs")) || Wanted.Contains(TEXT("..")) || Wanted.Contains(TEXT("://")))
	{
		UE_LOG(LogStudioDocs, Warning, TEXT("refused to open docs path %s"), *Wanted);
		return false;
	}
	const FString Url = Site + Wanted;
	if (const TSharedPtr<SWebBrowser> Browser = OpenBrowser.Pin())
	{
		Browser->LoadURL(Url);
		FGlobalTabmanager::Get()->TryInvokeTab(DocsTab);
		return true;
	}
	PendingUrl = Url;
	FGlobalTabmanager::Get()->TryInvokeTab(DocsTab);
	return true;
}

#undef LOCTEXT_NAMESPACE
