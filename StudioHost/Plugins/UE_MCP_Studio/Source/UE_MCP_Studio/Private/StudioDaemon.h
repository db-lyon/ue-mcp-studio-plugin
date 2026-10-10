// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** A running ue-mcp daemon for this project, read from its discovery file. */
struct FStudioDaemonInfo
{
	int32 Pid = 0;
	int32 Port = 0;
	FString Token;
	FString Version;
};

/** The init choices the setup page offers. Clients are ue-mcp client ids (claude-code, codex, ...). */
struct FStudioSetupOptions
{
	TArray<FString> Clients;
	bool bNativeTools = true;
	FString Context;
	bool bFeedback = false;
	bool bSkills = false;
};

/**
 * Finding, starting and talking to the project's ue-mcp daemon.
 *
 * The daemon publishes ~/.ue-mcp/daemons/<key>.json, where key is the first 16
 * hex digits of the SHA-1 of the normalized project root. The normalization is
 * the one ue-mcp's port.ts and the bridge's BridgeStateFiles.cpp use:
 * forward slashes, no trailing slash, lowercase.
 */
class FStudioDaemon
{
public:
	/** This project's root in ue-mcp's normalized form. */
	static FString NormalizedProjectRoot();

	static FString DiscoveryPath();

	/** The daemon for this project, when its discovery file names a live process. */
	static bool ReadLive(FStudioDaemonInfo& Out);

	/** Full path of the node executable, or empty when Node is not installed. */
	static FString FindNode();

	/** Whether the ue-mcp bridge plugin is present in this project or the engine. */
	static bool IsBridgeInstalled();

	/** ue-mcp@<the installed bridge's version>, so the server and the bridge move together; the release manifest's ueMcp.package when there is no bridge or it is older than ueMcp.min. */
	static FString PackageSpec();

	/**
	 * Start the daemon with `npx ue-mcp daemon start <uproject>`, detached and
	 * outside any job object the editor would kill it with. Returns false with
	 * OutError when the process could not be created.
	 */
	static bool StartDaemon(FString& OutError);

	/** Install or adopt the bridge with `npx ue-mcp init --yes --json`. Output goes to the Studio log. */
	static bool InstallBridge(FString& OutError);

	/**
	 * Run `ue-mcp init --dry-run` in the background; its JSON plan (the project's current
	 * choices and the MCP clients found on this machine) lands at SetupPlanPath().
	 */
	static bool StartSetupPlan(FString& OutError);

	/** Where StartSetupPlan writes the plan. Absent or partial until the run finishes. */
	static FString SetupPlanPath();

	/** `ue-mcp init` with the choices made on the setup page. */
	static bool RunSetup(const FStudioSetupOptions& Options, FString& OutError);

	/** The UI address, carrying the token the daemon exchanges for a cookie. */
	static FString UiUrl(const FStudioDaemonInfo& Info);

	/** Where Studio writes the output of the processes it starts. */
	static FString LogPath();

	/** Where daemon extensions live: UE_MCP_EXTENSIONS_DIR, else ~/.ue-mcp/extensions. */
	static FString ExtensionsDir();

	/** Version of the installed Studio extension, or empty when it is not installed. */
	static FString InstalledExtensionVersion();

	/** The release manifest the extension installs from. UE_MCP_STUDIO_CHANNEL overrides it. */
	static FString ExtensionManifestUrl();

	/** The last release manifest fetched, kept so the ue-mcp pin is known before the network answers. */
	static FString ManifestCachePath();

	/** Install or update the Studio extension, then start the daemon, as one detached process. */
	static bool InstallExtensionAndStart(FString& OutError);

	/**
	 * Bring Studio up: the extension installed and current, the daemon running.
	 * Checks the release channel for a newer extension in the background.
	 */
	static void EnsureRunning();

	/** The kind in this project's bridge install marker ("source" or "binary"), or empty. */
	static FString BridgeMarkerKind();

	/**
	 * Mount and load a bridge installed as binaries while the editor runs, so a
	 * first install needs no restart. Source installs need a build and a restart.
	 */
	static bool LoadInstalledBridge(FString& OutError);

	/** Set when the installed bridge cannot load without an editor restart. */
	static bool bBridgeNeedsRestart;

private:
	/** Run each command in turn, detached. Output appends to the Studio log, or stdout goes to StdoutFile when given. */
	static bool SpawnNpx(const TArray<FString>& Commands, FString& OutError, const FString& StdoutFile = FString());
};
