// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioWindow.h"

#include "StudioConsent.h"
#include "StudioDaemon.h"
#include "StudioDock.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/StyleColors.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogStudioWindow, Log, All);

namespace
{
	/** How long an open waits for the daemon to come up. */
	constexpr double StartTimeoutSeconds = 120.0;

	FTSTicker::FDelegateHandle PendingTicker;
	double PendingSince = 0.0;
	FString PendingMode;
	/** A daemon the pending open waits to see replaced (one restarting to load Studio), or 0. */
	int32 PendingStalePid = 0;
	FString PublishedTheme;

	void RequestWindow(const FStudioDaemonInfo& Info, const FString& Mode);

	bool PollPending(float)
	{
		FStudioDaemonInfo Info;
		if (FStudioDaemon::ReadLive(Info) && Info.Pid != PendingStalePid)
		{
			PendingTicker.Reset();
			PendingStalePid = 0;
			RequestWindow(Info, PendingMode);
			return false;
		}
		if (FPlatformTime::Seconds() - PendingSince > StartTimeoutSeconds)
		{
			UE_LOG(LogStudioWindow, Warning, TEXT("the daemon did not come up; Studio was not opened. %s has the details."), *FStudioDaemon::LogPath());
			PendingTicker.Reset();
			PendingStalePid = 0;
			return false;
		}
		return true;
	}

	/** Open the window once a daemon other than StalePid (0 for any) is up. The latest ask's mode wins. */
	void WaitForDaemon(const FString& Mode, int32 StalePid)
	{
		PendingMode = Mode;
		if (PendingTicker.IsValid())
		{
			return;
		}
		PendingStalePid = StalePid;
		PendingSince = FPlatformTime::Seconds();
		PendingTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PollPending), 0.5f);
	}

	/** Ask the daemon to open the window. Without the Studio extension loaded, restart the daemon with it, or show the setup page. */
	void RequestWindow(const FStudioDaemonInfo& Info, const FString& Mode)
	{
#if PLATFORM_WINDOWS
		// The editor is in front; let the window it is about to start come forward too.
		::AllowSetForegroundWindow(ASFW_ANY);
#endif
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(FString::Printf(TEXT("http://127.0.0.1:%d/v1/ext/studio/window"), Info.Port));
		Request->SetVerb(TEXT("POST"));
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Info.Token);
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		// Older Studio extensions ignore the mode and open their window.
		const FString ModeField = Mode.IsEmpty() ? FString() : FString::Printf(TEXT(",\"mode\":\"%s\""), *Mode);
		Request->SetContentAsString(FString::Printf(TEXT("{\"editorPid\":%u,\"fromEditor\":true%s}"), FPlatformProcess::GetCurrentProcessId(), *ModeField));
		Request->OnProcessRequestComplete().BindLambda([Info, Mode](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
		{
			const int32 Code = bOk && Response.IsValid() ? Response->GetResponseCode() : 0;
			if (Code == 200)
			{
				UE_LOG(LogStudioWindow, Log, TEXT("Studio window: %s"), *Response->GetContentAsString());
				return;
			}
			// The browser page needs the extension too: it holds the UI and the key the daemon trusts it by.
			FStudioDaemon::CheckExtensionLoaded(Info, [Info, Mode, Code](bool bLoaded, int32 AboutCode)
			{
				if (bLoaded)
				{
					// An extension older than the window, or one with nothing to start.
					UE_LOG(LogStudioWindow, Log, TEXT("the Studio window route answered %d; opening Studio in the browser"), Code);
					FPlatformProcess::LaunchURL(*FStudioDaemon::UiUrl(Info), nullptr, nullptr);
					return;
				}
				FString Error;
				if (AboutCode == 404 && FStudioDaemon::InstallExtensionOnce(Error))
				{
					UE_LOG(LogStudioWindow, Log, TEXT("the daemon on port %d has not loaded Studio; it restarts with it and Studio opens then"), Info.Port);
					WaitForDaemon(Mode, Info.Pid);
					return;
				}
				UE_LOG(LogStudioWindow, Warning, TEXT("the daemon on port %d cannot serve Studio (window %d, about %d) %s; showing the setup page"), Info.Port, Code, AboutCode, *Error);
				FStudioDock::OpenSetupPage();
			});
		});
		Request->ProcessRequest();
	}
}

bool FStudioWindow::Open(const TCHAR* Mode)
{
	const FString ModeName = Mode ? FString(Mode) : FString();
	FStudioDaemonInfo Info;
	const bool bLive = FStudioDaemon::ReadLive(Info);
	// A live daemon is not enough: one another MCP client started has no Studio until it is installed with consent.
	const EStudioOpenStep Step = FStudioDaemon::NextOpenStep(FStudioConsent::IsGranted(), !FStudioDaemon::InstalledExtensionVersion().IsEmpty(), bLive);
	if (Step == EStudioOpenStep::Open)
	{
		RequestWindow(Info, ModeName);
		return true;
	}
	// Without install consent the dock tab's setup page asks for it.
	if (Step == EStudioOpenStep::AskConsent || !FStudioDaemon::IsBridgeInstalled() || FStudioDaemon::FindNode().IsEmpty())
	{
		return false;
	}
	if (PendingTicker.IsValid())
	{
		PendingMode = ModeName;
		return true;
	}
	FString Error;
	// The install restarts a running daemon so it loads Studio, so the open waits for its successor.
	const bool bStarted = Step == EStudioOpenStep::Install
		? FStudioDaemon::InstallExtensionOnce(Error)
		: FStudioDaemon::StartDaemon(Error);
	if (!bStarted)
	{
		UE_LOG(LogStudioWindow, Warning, TEXT("the daemon did not start: %s"), *Error);
		return false;
	}
	WaitForDaemon(ModeName, bLive ? Info.Pid : 0);
	return true;
}

bool FStudioWindow::IsOpen()
{
#if PLATFORM_WINDOWS
	// The window keeps a message window named for the project; finding it sends nothing to the window.
	const FString Class = FString(TEXT("UEMCPStudioWindow.")) + FStudioDaemon::ProjectKey();
	return ::FindWindowExW(HWND_MESSAGE, nullptr, *Class, nullptr) != nullptr;
#else
	return false;
#endif
}

FString FStudioWindow::EditorThemeJson()
{
	struct FNamed { const TCHAR* Name; EStyleColor Color; };
	static const FNamed Colors[] = {
		{ TEXT("background"), EStyleColor::Background },
		{ TEXT("recessed"), EStyleColor::Recessed },
		{ TEXT("panel"), EStyleColor::Panel },
		{ TEXT("header"), EStyleColor::Header },
		{ TEXT("input"), EStyleColor::Input },
		{ TEXT("inputOutline"), EStyleColor::InputOutline },
		{ TEXT("dropdown"), EStyleColor::Dropdown },
		{ TEXT("dropdownOutline"), EStyleColor::DropdownOutline },
		{ TEXT("hover"), EStyleColor::Hover },
		{ TEXT("hover2"), EStyleColor::Hover2 },
		{ TEXT("foreground"), EStyleColor::Foreground },
		{ TEXT("foregroundHover"), EStyleColor::ForegroundHover },
		{ TEXT("foregroundHeader"), EStyleColor::ForegroundHeader },
		{ TEXT("primary"), EStyleColor::Primary },
		{ TEXT("primaryHover"), EStyleColor::PrimaryHover },
		{ TEXT("select"), EStyleColor::Select },
		{ TEXT("highlight"), EStyleColor::Highlight },
		{ TEXT("warning"), EStyleColor::Warning },
		{ TEXT("error"), EStyleColor::Error },
		{ TEXT("success"), EStyleColor::Success },
	};
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	for (const FNamed& C : Colors)
	{
		const FColor Srgb = USlateThemeManager::Get().GetColor(C.Color).ToFColorSRGB();
		Out->SetStringField(C.Name, FString::Printf(TEXT("#%02x%02x%02x"), Srgb.R, Srgb.G, Srgb.B));
	}
	FString Text;
	FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

void FStudioWindow::PublishEditorTheme()
{
	const FString Theme = EditorThemeJson();
	if (Theme == PublishedTheme)
	{
		return;
	}
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UE_MCP_Studio"), TEXT("editor-theme.json")));
	if (FFileHelper::SaveStringToFile(Theme, *File))
	{
		PublishedTheme = Theme;
	}
}

void FStudioWindow::Shutdown()
{
	if (PendingTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingTicker);
		PendingTicker.Reset();
	}
}
