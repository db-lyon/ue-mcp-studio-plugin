// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioDaemon.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IProjectManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogStudio, Log, All);

bool FStudioDaemon::bBridgeNeedsRestart = false;

namespace
{
	// Used only until the first release manifest is fetched; after that the
	// manifest's ueMcp.min and ueMcp.package decide, so the host never needs an
	// update to follow a ue-mcp release.
	const TCHAR* FallbackMinUeMcpVersion = TEXT("1.4.0-beta.1");
	const TCHAR* FallbackUeMcpPackage = TEXT("ue-mcp@beta");

	/** Semver order over X.Y.Z[-pre.N]; a prerelease sorts below its release. */
	int32 CompareVersions(const FString& A, const FString& B)
	{
		auto Split = [](const FString& V, TArray<int32>& Core, TArray<FString>& Pre)
		{
			FString Main = V, Tail;
			V.Split(TEXT("-"), &Main, &Tail);
			TArray<FString> Parts;
			Main.ParseIntoArray(Parts, TEXT("."));
			for (int32 i = 0; i < 3; ++i) Core.Add(Parts.IsValidIndex(i) ? FCString::Atoi(*Parts[i]) : 0);
			if (!Tail.IsEmpty()) Tail.ParseIntoArray(Pre, TEXT("."));
		};
		TArray<int32> CA, CB;
		TArray<FString> PA, PB;
		Split(A, CA, PA);
		Split(B, CB, PB);
		for (int32 i = 0; i < 3; ++i)
		{
			if (CA[i] != CB[i]) return CA[i] < CB[i] ? -1 : 1;
		}
		if (PA.Num() == 0 || PB.Num() == 0) return PA.Num() == PB.Num() ? 0 : (PA.Num() == 0 ? 1 : -1);
		for (int32 i = 0; i < FMath::Min(PA.Num(), PB.Num()); ++i)
		{
			const bool bNumA = PA[i].IsNumeric(), bNumB = PB[i].IsNumeric();
			const int32 C = bNumA && bNumB ? FCString::Atoi(*PA[i]) - FCString::Atoi(*PB[i]) : PA[i].Compare(PB[i]);
			if (C != 0) return C < 0 ? -1 : 1;
		}
		return PA.Num() == PB.Num() ? 0 : (PA.Num() < PB.Num() ? -1 : 1);
	}

	FString UserHome()
	{
#if PLATFORM_WINDOWS
		return FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
#else
		return FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
#endif
	}

	FString ProjectFile()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	}

	/** The PATH entries, split on the platform separator. */
	TArray<FString> PathEntries()
	{
		TArray<FString> Out;
		FPlatformMisc::GetEnvironmentVariable(TEXT("PATH")).ParseIntoArray(Out, PLATFORM_WINDOWS ? TEXT(";") : TEXT(":"));
		return Out;
	}
}

FString FStudioDaemon::NormalizedProjectRoot()
{
	FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	Root.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Root.EndsWith(TEXT("/")))
	{
		Root.LeftChopInline(1);
	}
	return Root.ToLower();
}

FString FStudioDaemon::DiscoveryPath()
{
	FString Dir = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_MCP_DAEMON_DIR"));
	if (Dir.IsEmpty())
	{
		Dir = FPaths::Combine(UserHome(), TEXT(".ue-mcp"), TEXT("daemons"));
	}
	const FTCHARToUTF8 Utf8(*NormalizedProjectRoot());
	FSHAHash Hash;
	FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
	const FString Key = BytesToHex(Hash.Hash, sizeof(Hash.Hash)).ToLower().Left(16);
	return FPaths::Combine(Dir, Key + TEXT(".json"));
}

bool FStudioDaemon::ReadLive(FStudioDaemonInfo& Out)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *DiscoveryPath()))
	{
		return false;
	}
	TSharedPtr<FJsonObject> Json;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json.IsValid())
	{
		return false;
	}
	FString Root;
	if (!Json->TryGetStringField(TEXT("projectRoot"), Root) || Root != NormalizedProjectRoot())
	{
		return false;
	}
	FStudioDaemonInfo Info;
	Info.Pid = static_cast<int32>(Json->GetNumberField(TEXT("pid")));
	Info.Port = static_cast<int32>(Json->GetNumberField(TEXT("port")));
	Json->TryGetStringField(TEXT("token"), Info.Token);
	Json->TryGetStringField(TEXT("version"), Info.Version);
	if (Info.Pid <= 0 || Info.Port <= 0 || Info.Token.IsEmpty() || !FPlatformProcess::IsApplicationRunning(Info.Pid))
	{
		return false;
	}
	Out = Info;
	return true;
}

FString FStudioDaemon::FindNode()
{
	const FString Exe = PLATFORM_WINDOWS ? TEXT("node.exe") : TEXT("node");
	for (const FString& Dir : PathEntries())
	{
		const FString Candidate = FPaths::Combine(Dir.TrimQuotes(), Exe);
		if (IFileManager::Get().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
	// A GUI launch may not carry the PATH a shell would, so look where installers put it.
	TArray<FString> Known;
#if PLATFORM_WINDOWS
	Known.Add(FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("ProgramFiles")), TEXT("nodejs"), Exe));
	Known.Add(FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("NVM_SYMLINK")), Exe));
#else
	Known.Add(TEXT("/usr/local/bin/node"));
	Known.Add(TEXT("/opt/homebrew/bin/node"));
	Known.Add(TEXT("/usr/bin/node"));
#endif
	for (const FString& Candidate : Known)
	{
		if (IFileManager::Get().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

bool FStudioDaemon::IsBridgeInstalled()
{
	// Mounted, not merely discovered: a plugin added to the list but not loaded is not usable yet.
	const TSharedPtr<IPlugin> Bridge = IPluginManager::Get().FindPlugin(TEXT("UE_MCP_Bridge"));
	return Bridge.IsValid() && Bridge->IsMounted();
}

FString FStudioDaemon::PackageSpec()
{
	// For development: any spec npx accepts, such as a local ue-mcp checkout.
	const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_MCP_STUDIO_PACKAGE"));
	if (!Override.IsEmpty())
	{
		return FString::Printf(TEXT("\"%s\""), *Override);
	}
	FString MinVersion = FallbackMinUeMcpVersion;
	FString Package = FallbackUeMcpPackage;
	FString Text;
	TSharedPtr<FJsonObject> Manifest;
	const TSharedPtr<FJsonObject>* UeMcp = nullptr;
	if (FFileHelper::LoadFileToString(Text, *ManifestCachePath())
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest) && Manifest.IsValid()
		&& Manifest->TryGetObjectField(TEXT("ueMcp"), UeMcp))
	{
		(*UeMcp)->TryGetStringField(TEXT("min"), MinVersion);
		(*UeMcp)->TryGetStringField(TEXT("package"), Package);
	}
	// The bridge's own version keeps server and bridge in step, unless it is older than Studio needs.
	const TSharedPtr<IPlugin> Bridge = IPluginManager::Get().FindPlugin(TEXT("UE_MCP_Bridge"));
	const FString Version = Bridge.IsValid() ? Bridge->GetDescriptor().VersionName : FString();
	return Version.IsEmpty() || CompareVersions(Version, MinVersion) < 0
		? Package
		: FString::Printf(TEXT("ue-mcp@%s"), *Version);
}

FString FStudioDaemon::ManifestCachePath()
{
	return FPaths::Combine(UserHome(), TEXT(".ue-mcp"), TEXT("studio"), TEXT("latest.json"));
}

FString FStudioDaemon::LogPath()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Logs"), TEXT("UE_MCP_Studio.log")));
}

FString FStudioDaemon::UiUrl(const FStudioDaemonInfo& Info)
{
	return FString::Printf(TEXT("http://127.0.0.1:%d/ui/?token=%s"), Info.Port, *Info.Token);
}

bool FStudioDaemon::StartDaemon(FString& OutError)
{
	return SpawnNpx({ FString::Printf(TEXT("-y %s daemon start \"%s\""), *PackageSpec(), *ProjectFile()) }, OutError);
}

FString FStudioDaemon::ExtensionsDir()
{
	const FString Dir = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_MCP_EXTENSIONS_DIR"));
	return Dir.IsEmpty() ? FPaths::Combine(UserHome(), TEXT(".ue-mcp"), TEXT("extensions")) : Dir;
}

FString FStudioDaemon::InstalledExtensionVersion()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(ExtensionsDir(), TEXT("studio"), TEXT("package.json"))))
	{
		return FString();
	}
	TSharedPtr<FJsonObject> Json;
	FString Version;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid())
	{
		Json->TryGetStringField(TEXT("version"), Version);
	}
	return Version;
}

FString FStudioDaemon::ExtensionManifestUrl()
{
	const FString Override = FPlatformMisc::GetEnvironmentVariable(TEXT("UE_MCP_STUDIO_CHANNEL"));
	return Override.IsEmpty()
		? FString(TEXT("https://github.com/db-lyon/ue-mcp-studio-releases/releases/latest/download/latest.json"))
		: Override;
}

bool FStudioDaemon::InstallExtensionAndStart(FString& OutError)
{
	return SpawnNpx({
		FString::Printf(TEXT("-y %s extension install \"%s\" \"%s\""), *PackageSpec(), *ExtensionManifestUrl(), *ProjectFile()),
		FString::Printf(TEXT("-y %s daemon start \"%s\""), *PackageSpec(), *ProjectFile()),
	}, OutError);
}

void FStudioDaemon::EnsureRunning()
{
	if (!IsBridgeInstalled() || FindNode().IsEmpty())
	{
		return;
	}
	FString Error;
	const FString Installed = InstalledExtensionVersion();
	if (Installed.IsEmpty())
	{
		if (!InstallExtensionAndStart(Error))
		{
			UE_LOG(LogStudio, Warning, TEXT("could not install Studio: %s"), *Error);
		}
		return;
	}
	FStudioDaemonInfo Info;
	if (!ReadLive(Info) && !StartDaemon(Error))
	{
		UE_LOG(LogStudio, Warning, TEXT("daemon did not start: %s"), *Error);
	}

	// A newer release replaces the extension; the install restarts the daemon to load it.
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ExtensionManifestUrl());
	Request->SetVerb(TEXT("GET"));
	Request->OnProcessRequestComplete().BindLambda([Installed](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
	{
		if (!bOk || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			return;
		}
		const FString Body = Response->GetContentAsString();
		TSharedPtr<FJsonObject> Json;
		FString Latest;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Json) || !Json.IsValid()
			|| !Json->TryGetStringField(TEXT("version"), Latest) || Latest.IsEmpty())
		{
			return;
		}
		// Kept for the next start, when PackageSpec needs the ue-mcp pin before any network reply.
		FFileHelper::SaveStringToFile(Body, *ManifestCachePath());
		if (Latest == Installed)
		{
			return;
		}
		UE_LOG(LogStudio, Log, TEXT("updating Studio %s -> %s"), *Installed, *Latest);
		FString UpdateError;
		if (!InstallExtensionAndStart(UpdateError))
		{
			UE_LOG(LogStudio, Warning, TEXT("could not update Studio: %s"), *UpdateError);
		}
	});
	Request->ProcessRequest();
}

FString FStudioDaemon::BridgeMarkerKind()
{
	FString Text;
	const FString Marker = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UE_MCP_Bridge"), TEXT(".ue-mcp-install.json"));
	if (!FFileHelper::LoadFileToString(Text, *Marker))
	{
		return FString();
	}
	TSharedPtr<FJsonObject> Json;
	FString Kind;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid())
	{
		Json->TryGetStringField(TEXT("kind"), Kind);
	}
	return Kind;
}

bool FStudioDaemon::LoadInstalledBridge(FString& OutError)
{
	if (IsBridgeInstalled())
	{
		return true;
	}
	if (BridgeMarkerKind() != TEXT("binary"))
	{
		OutError = TEXT("the bridge is not installed as binaries; restart the editor to build it");
		return false;
	}
	// init enabled the plugins in the .uproject on disk; the editor's copy has to agree,
	// or its next save of the descriptor would drop them.
	FString Text;
	TSharedPtr<FJsonObject> Descriptor;
	if (FFileHelper::LoadFileToString(Text, *ProjectFile())
		&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Descriptor) && Descriptor.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Plugins = nullptr;
		if (Descriptor->TryGetArrayField(TEXT("Plugins"), Plugins))
		{
			for (const TSharedPtr<FJsonValue>& Entry : *Plugins)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				FString Name;
				bool bEnabled = false;
				if (Entry->TryGetObject(Obj) && (*Obj)->TryGetStringField(TEXT("Name"), Name) && (*Obj)->TryGetBoolField(TEXT("Enabled"), bEnabled) && bEnabled)
				{
					FText Ignored;
					IProjectManager::Get().SetPluginEnabled(Name, true, Ignored);
				}
			}
		}
	}
	// The plugin manager only knows plugins found at startup, so the new one is added to its list first.
	const FString BridgeDescriptor = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UE_MCP_Bridge"), TEXT("UE_MCP_Bridge.uplugin")));
	FText AddError;
	if (!IPluginManager::Get().AddToPluginsList(BridgeDescriptor, &AddError))
	{
		OutError = FString::Printf(TEXT("the bridge plugin could not be read: %s"), *AddError.ToString());
		return false;
	}
	// Engine plugins mounted late can assert (GameplayAbilitiesEditor does on 5.8), so the bridge
	// loads now only when everything it depends on is already loaded; otherwise one restart.
	const TSharedPtr<IPlugin> Bridge = IPluginManager::Get().FindPlugin(TEXT("UE_MCP_Bridge"));
	for (const FPluginReferenceDescriptor& Dep : Bridge->GetDescriptor().Plugins)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(Dep.Name);
		if (Dep.bEnabled && !Dep.bOptional && !(Plugin.IsValid() && Plugin->IsMounted()))
		{
			bBridgeNeedsRestart = true;
			OutError = FString::Printf(TEXT("the bridge needs %s, which loads only at editor start; restart the editor"), *Dep.Name);
			return false;
		}
	}
	IPluginManager::Get().MountNewlyCreatedPlugin(TEXT("UE_MCP_Bridge"));
	if (!IsBridgeInstalled())
	{
		OutError = TEXT("the bridge plugin did not mount");
		return false;
	}
	UE_LOG(LogStudio, Log, TEXT("loaded the newly installed bridge without a restart"));
	return true;
}

bool FStudioDaemon::InstallBridge(FString& OutError)
{
	// No bridge yet, so no version to pin. The client path is always binaries (spec section 8), and
	// Studio is the MCP client, so init leaves other clients' configs alone.
	return SpawnNpx({ FString::Printf(TEXT("-y %s init --yes --json --install=binary --clients= --project=\"%s\""), *PackageSpec(), *ProjectFile()) }, OutError);
}

bool FStudioDaemon::SpawnNpx(const TArray<FString>& Commands, FString& OutError)
{
	const FString Node = FindNode();
	if (Node.IsEmpty())
	{
		OutError = TEXT("Node.js is not installed");
		return false;
	}
	const FString NodeDir = FPaths::GetPath(Node);
	const FString Log = LogPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Log), true);
	for (const FString& Arguments : Commands)
	{
		UE_LOG(LogStudio, Log, TEXT("npx %s"), *Arguments);
	}

#if PLATFORM_WINDOWS
	// npx is a .cmd script, so it runs under cmd. Created with CreateProcessW rather
	// than FPlatformProcess::CreateProc, which places children in a job object the
	// editor closes on exit; the daemon has to outlive the editor.
	// Native separators throughout: cmd reads "C:\WINDOWS/System32/cmd.exe" as an
	// internal command plus arguments, and runs md on them.
	auto Native = [](FString P) { FPaths::MakePlatformFilename(P); return P; };
	const FString Npx = Native(FPaths::Combine(NodeDir, TEXT("npx.cmd")));
	const FString Cmd = Native(FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot")), TEXT("System32"), TEXT("cmd.exe")));
	const FString WorkDir = Native(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	// Each step runs only when the one before it succeeded.
	TArray<FString> Steps;
	for (const FString& Arguments : Commands)
	{
		Steps.Add(FString::Printf(TEXT("\"%s\" %s >> \"%s\" 2>&1"), *Npx, *Arguments, *Native(Log)));
	}
	FString CommandLine = FString::Printf(TEXT("\"%s\" /d /s /c \"%s\""), *Cmd, *FString::Join(Steps, TEXT(" && ")));

	// node and npx must resolve even when the editor was started without them on PATH.
	const FString Path = NodeDir + TEXT(";") + FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	FPlatformMisc::SetEnvironmentVar(TEXT("PATH"), *Path);

	STARTUPINFOW Startup;
	FMemory::Memzero(Startup);
	Startup.cb = sizeof(Startup);
	PROCESS_INFORMATION Process;
	FMemory::Memzero(Process);
	DWORD Flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | CREATE_BREAKAWAY_FROM_JOB;
	BOOL bOk = ::CreateProcessW(*Cmd, CommandLine.GetCharArray().GetData(), nullptr, nullptr, false, Flags, nullptr, *WorkDir, &Startup, &Process);
	if (!bOk && ::GetLastError() == ERROR_ACCESS_DENIED)
	{
		// The editor is in a job that does not allow breakaway; start it inside the job rather than not at all.
		Flags &= ~CREATE_BREAKAWAY_FROM_JOB;
		bOk = ::CreateProcessW(*Cmd, CommandLine.GetCharArray().GetData(), nullptr, nullptr, false, Flags, nullptr, *WorkDir, &Startup, &Process);
	}
	if (!bOk)
	{
		OutError = FString::Printf(TEXT("CreateProcess failed (%u)"), ::GetLastError());
		return false;
	}
	::CloseHandle(Process.hThread);
	::CloseHandle(Process.hProcess);
	return true;
#else
	const FString Npx = FPaths::Combine(NodeDir, TEXT("npx"));
	TArray<FString> Steps;
	for (const FString& Arguments : Commands)
	{
		Steps.Add(FString::Printf(TEXT("'%s' %s >> '%s' 2>&1"), *Npx, *Arguments.Replace(TEXT("\""), TEXT("'")), *Log));
	}
	const FString Shell = FString::Printf(TEXT("-c \"PATH='%s':$PATH; %s\""), *NodeDir, *FString::Join(Steps, TEXT(" && ")));
	FProcHandle Handle = FPlatformProcess::CreateProc(TEXT("/bin/sh"), *Shell, true, true, true, nullptr, 0, *FPaths::ProjectDir(), nullptr);
	if (!Handle.IsValid())
	{
		OutError = TEXT("could not start /bin/sh");
		return false;
	}
	FPlatformProcess::CloseProc(Handle);
	return true;
#endif
}
