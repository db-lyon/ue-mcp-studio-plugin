// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioNativeBinding.h"

#include "StudioDaemon.h"
#include "StudioDocs.h"
#include "Dom/JsonObject.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SWindow.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/StyleColors.h"
#include "UnrealEdMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogStudioBinding, Log, All);

FString UStudioNativeBinding::GetStatus()
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("node"), !FStudioDaemon::FindNode().IsEmpty());
	Out->SetBoolField(TEXT("bridgeInstalled"), FStudioDaemon::IsBridgeInstalled());
	Out->SetStringField(TEXT("log"), FStudioDaemon::LogPath());
	Out->SetStringField(TEXT("bridgeOnDisk"), FStudioDaemon::BridgeMarkerKind());
	Out->SetBoolField(TEXT("bridgeNeedsRestart"), FStudioDaemon::bBridgeNeedsRestart);
	Out->SetStringField(TEXT("studioVersion"), FStudioDaemon::InstalledExtensionVersion());
	FStudioDaemonInfo Info;
	if (FStudioDaemon::ReadLive(Info))
	{
		TSharedRef<FJsonObject> Daemon = MakeShared<FJsonObject>();
		Daemon->SetNumberField(TEXT("port"), Info.Port);
		Daemon->SetStringField(TEXT("version"), Info.Version);
		Out->SetObjectField(TEXT("daemon"), Daemon);
	}
	else
	{
		Out->SetField(TEXT("daemon"), MakeShared<FJsonValueNull>());
	}
	FString Text;
	FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

bool UStudioNativeBinding::StartDaemon()
{
	FString Error;
	if (FStudioDaemon::InstalledExtensionVersion().IsEmpty())
	{
		if (!FStudioDaemon::InstallExtensionAndStart(Error))
		{
			UE_LOG(LogStudioBinding, Warning, TEXT("Studio did not install: %s"), *Error);
			return false;
		}
		return true;
	}
	if (!FStudioDaemon::StartDaemon(Error))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("daemon did not start: %s"), *Error);
		return false;
	}
	return true;
}

bool UStudioNativeBinding::InstallBridge()
{
	FString Error;
	if (!FStudioDaemon::InstallBridge(Error))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("bridge install did not start: %s"), *Error);
		return false;
	}
	return true;
}

bool UStudioNativeBinding::StartSetupPlan()
{
	FString Error;
	if (!FStudioDaemon::StartSetupPlan(Error))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("setup plan did not start: %s"), *Error);
		return false;
	}
	return true;
}

FString UStudioNativeBinding::GetSetupPlan()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FStudioDaemon::SetupPlanPath()))
	{
		return FString();
	}
	// The file fills in as init writes it; only a complete document counts.
	TSharedPtr<FJsonObject> Parsed;
	return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Parsed) && Parsed.IsValid() ? Text : FString();
}

bool UStudioNativeBinding::RunSetup(const FString& OptionsJson)
{
	static const TSet<FString> KnownClients = {
		TEXT("claude-code"), TEXT("claude-code-global"), TEXT("claude-desktop"), TEXT("cursor"), TEXT("codex"),
	};
	static const TSet<FString> KnownContexts = { TEXT("micro"), TEXT("lean"), TEXT("full") };

	TSharedPtr<FJsonObject> In;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(OptionsJson), In) || !In.IsValid())
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("setup options are not JSON"));
		return false;
	}
	FStudioSetupOptions Options;
	const TArray<TSharedPtr<FJsonValue>>* Clients = nullptr;
	if (In->TryGetArrayField(TEXT("clients"), Clients))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Clients)
		{
			const FString Id = Value->AsString();
			if (!KnownClients.Contains(Id))
			{
				UE_LOG(LogStudioBinding, Warning, TEXT("refused unknown MCP client '%s'"), *Id);
				return false;
			}
			Options.Clients.AddUnique(Id);
		}
	}
	In->TryGetBoolField(TEXT("nativeTools"), Options.bNativeTools);
	In->TryGetBoolField(TEXT("feedback"), Options.bFeedback);
	In->TryGetBoolField(TEXT("skills"), Options.bSkills);
	if (In->TryGetStringField(TEXT("context"), Options.Context) && !KnownContexts.Contains(Options.Context))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("refused unknown context strategy '%s'"), *Options.Context);
		return false;
	}

	FString Error;
	if (!FStudioDaemon::RunSetup(Options, Error))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("setup did not start: %s"), *Error);
		return false;
	}
	return true;
}

bool UStudioNativeBinding::RestartEditor()
{
	FUnrealEdMisc::Get().RestartEditor(false);
	return true;
}

bool UStudioNativeBinding::EnablePlugin(const FString& Name)
{
	// The plugins ue-mcp init enables, and nothing else.
	static const TSet<FString> Allowed = {
		TEXT("UE_MCP_Bridge"), TEXT("PythonScriptPlugin"), TEXT("Niagara"), TEXT("PCG"),
		TEXT("EnhancedInput"), TEXT("GameplayAbilities"),
	};
	if (!Allowed.Contains(Name))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("refused to enable plugin '%s'"), *Name);
		return false;
	}
	FText Failure;
	if (!IProjectManager::Get().SetPluginEnabled(Name, true, Failure) || !IProjectManager::Get().SaveCurrentProjectToDisk(Failure))
	{
		UE_LOG(LogStudioBinding, Warning, TEXT("could not enable '%s': %s"), *Name, *Failure.ToString());
		return false;
	}
	return true;
}

bool UStudioNativeBinding::OpenProjectFolder()
{
	FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	return true;
}

bool UStudioNativeBinding::OpenNodeDownload()
{
	FPlatformProcess::LaunchURL(TEXT("https://nodejs.org/en/download"), nullptr, nullptr);
	return true;
}

bool UStudioNativeBinding::OpenDocs(const FString& Path)
{
	return FStudioDocs::Open(Path);
}

bool UStudioNativeBinding::DrawAttention(const FString& Message)
{
	// The page writes the text; keep it to one short line.
	FNotificationInfo Info(FText::FromString(Message.Left(160)));
	Info.ExpireDuration = 6.0f;
	Info.bFireAndForget = true;
	FSlateNotificationManager::Get().AddNotification(Info);

	const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
	if (Root.IsValid() && !Root->IsActive())
	{
		Root->DrawAttention(FWindowDrawAttentionParameters(EWindowDrawAttentionRequestType::UntilActivated));
	}
	return true;
}

FString UStudioNativeBinding::GetEditorTheme()
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
