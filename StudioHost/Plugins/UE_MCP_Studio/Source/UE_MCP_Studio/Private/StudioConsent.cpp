// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioConsent.h"

#include "StudioDaemon.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "UE_MCP_Studio"

DEFINE_LOG_CATEGORY_STATIC(LogStudioConsent, Log, All);

namespace StudioConsentImpl
{
	/** Granted is cached; a refusal is read again, so an Allow elsewhere is seen at once. */
	bool bGranted = false;
}

FString FStudioConsent::FilePath()
{
	return FPaths::Combine(FStudioDaemon::UserDir(), TEXT("studio"), TEXT("consent.json"));
}

EStudioConsent FStudioConsent::Evaluate(bool bFileExists, const FString& FileText, bool bStudioInstalled)
{
	if (!bFileExists)
	{
		return bStudioInstalled ? EStudioConsent::Adopt : EStudioConsent::Ask;
	}
	// An unreadable or older file asks again, even when Studio is installed.
	TSharedPtr<FJsonObject> Json;
	int32 Version = 0;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FileText), Json) && Json.IsValid()
		&& Json->TryGetNumberField(TEXT("version"), Version) && Version >= CurrentVersion)
	{
		return EStudioConsent::Granted;
	}
	return EStudioConsent::Ask;
}

FString FStudioConsent::MakeFileText(const TCHAR* Source)
{
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetNumberField(TEXT("version"), CurrentVersion);
	Out->SetStringField(TEXT("granted"), FDateTime::UtcNow().ToIso8601());
	Out->SetStringField(TEXT("source"), Source);
	FString Text;
	FJsonSerializer::Serialize(Out, TJsonWriterFactory<>::Create(&Text));
	return Text;
}

bool FStudioConsent::IsGranted()
{
	if (StudioConsentImpl::bGranted)
	{
		return true;
	}
	const FString File = FilePath();
	FString Text;
	const bool bExists = FFileHelper::LoadFileToString(Text, *File);
	switch (Evaluate(bExists, Text, !FStudioDaemon::InstalledExtensionVersion().IsEmpty()))
	{
	case EStudioConsent::Granted:
		StudioConsentImpl::bGranted = true;
		break;
	case EStudioConsent::Adopt:
		// Studio was installed before the prompt existed; that install keeps working.
		if (!FFileHelper::SaveStringToFile(MakeFileText(TEXT("existing-install")), *File))
		{
			UE_LOG(LogStudioConsent, Warning, TEXT("could not write %s"), *File);
		}
		StudioConsentImpl::bGranted = true;
		break;
	case EStudioConsent::Ask:
		break;
	}
	return StudioConsentImpl::bGranted;
}

bool FStudioConsent::Grant()
{
	const FString File = FilePath();
	StudioConsentImpl::bGranted = true;
	if (!FFileHelper::SaveStringToFile(MakeFileText(TEXT("prompt")), *File))
	{
		// Granted for this session regardless; the prompt returns next start.
		UE_LOG(LogStudioConsent, Warning, TEXT("could not write %s"), *File);
		return false;
	}
	return true;
}

TArray<FStudioInstallTarget> FStudioConsent::Targets()
{
	auto Full = [](const FString& Path)
	{
		FString Out = FPaths::ConvertRelativePathToFull(Path);
		FPaths::MakePlatformFilename(Out);
		return Out;
	};
	FString NpmCache = FPlatformMisc::GetEnvironmentVariable(TEXT("npm_config_cache"));
	if (NpmCache.IsEmpty())
	{
#if PLATFORM_WINDOWS
		NpmCache = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA")), TEXT("npm-cache"));
#else
		NpmCache = FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("HOME")), TEXT(".npm"));
#endif
	}
	return {
		{ LOCTEXT("ConsentBridge", "The UE-MCP bridge plugin, in this project"), Full(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UE_MCP_Bridge"))) },
		{ LOCTEXT("ConsentNpm", "The ue-mcp server from npm, run with npx and kept in npm's cache"), Full(FPaths::Combine(NpmCache, TEXT("_npx"))) },
		{ LOCTEXT("ConsentExtension", "The Studio extension for the ue-mcp server"), Full(FPaths::Combine(FStudioDaemon::ExtensionsDir(), TEXT("studio"))) },
		{ LOCTEXT("ConsentWindow", "Studio's window app"), Full(FPaths::Combine(FStudioDaemon::UserDir(), TEXT("studio"), TEXT("window"))) },
	};
}

#undef LOCTEXT_NAMESPACE
