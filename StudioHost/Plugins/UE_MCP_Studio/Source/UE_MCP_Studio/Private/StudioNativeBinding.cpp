// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioNativeBinding.h"

#include "StudioDaemon.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IProjectManager.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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
