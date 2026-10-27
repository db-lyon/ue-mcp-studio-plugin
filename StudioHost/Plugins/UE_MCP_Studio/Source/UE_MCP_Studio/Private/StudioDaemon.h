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

/**
 * The init choices the setup page offers, one per `ue-mcp init` flag. Clients are
 * ue-mcp client ids such as claude-code and codex; Disable and NativeExclude are tool
 * category names. Every field is passed, so init keeps nothing the page did not show.
 */
struct FStudioSetupOptions
{
	TArray<FString> Clients;
	TArray<FString> Disable;
	bool bNativeTools = true;
	TArray<FString> NativeExclude;
	FString Context;
	bool bFeedback = true;
	bool bHook = false;
	bool bSkills = false;
};

/** What opening Studio needs next. */
enum class EStudioOpenStep : uint8
{
	/** No install consent: the setup page asks, even when a daemon is running. */
	AskConsent,
	/** Install the Studio extension. The install restarts a running daemon so it loads it. */
	Install,
	/** Start the daemon. */
	Start,
	/** Ask the running daemon for Studio's window. */
	Open,
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

	/** The daemon's key for this project: the first 16 hex digits of the SHA-1 of NormalizedProjectRoot. */
	static FString ProjectKey();

	static FString DiscoveryPath();

	/** The daemon for this project, when its discovery file names a live process. */
	static bool ReadLive(FStudioDaemonInfo& Out);

	/**
	 * The daemon, when it can serve Studio: live, with install consent, the Studio extension
	 * installed and not known to be missing from it. A daemon another MCP client started
	 * does not count before that.
	 */
	static bool ReadReady(FStudioDaemonInfo& Out);

	/** The next step for an open, from consent, whether the extension is installed and whether a daemon is live. */
	static EStudioOpenStep NextOpenStep(bool bGranted, bool bExtensionInstalled, bool bLive);

	/** Whether the daemon has loaded the Studio extension: a daemon without it answers 404 under /v1/ext/studio. Calls back on the game thread with the status code. */
	static void CheckExtensionLoaded(const FStudioDaemonInfo& Info, TFunction<void(bool bLoaded, int32 Code)> Done);

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

	/** `ue-mcp init` with the choices made on the setup page. Its JSON result lands at SetupResultPath(). */
	static bool RunSetup(const FStudioSetupOptions& Options, FString& OutError);

	/** The ue-mcp init flags for Options, after the fixed ones. */
	static FString SetupFlags(const FStudioSetupOptions& Options);

	/** Where RunSetup writes init's result: ok, error and the steps it took. Absent or partial until it finishes. */
	static FString SetupResultPath();

	/** Run `ue-mcp auth --json`, the GitHub sign-in for feedback; its JSON lines land at FeedbackAuthPath(). */
	static bool StartFeedbackAuth(FString& OutError);

	/** Where StartFeedbackAuth writes the sign-in's progress, one JSON object per line. */
	static FString FeedbackAuthPath();

	/** The project's Studio settings, written by the Studio UI: ~/.ue-mcp/data/<key>/studio/settings.json. */
	static FString StudioSettingsPath();

	/** The UI address, carrying the token the daemon exchanges for a cookie. */
	static FString UiUrl(const FStudioDaemonInfo& Info);

	/** Where Studio writes the output of the processes it starts. */
	static FString LogPath();

	/** ~/.ue-mcp, where ue-mcp and Studio keep per-user state. */
	static FString UserDir();

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
	 * InstallExtensionAndStart, unless an install started in the last minute is still under way.
	 * The install restarts a running daemon, so it also loads the extension into one without it.
	 */
	static bool InstallExtensionOnce(FString& OutError);

	/**
	 * Hand a newer Studio to the running daemon's updater, which installs it now
	 * or, while a turn is in progress, once the turn finishes. Installs directly
	 * when the daemon has no updater to ask.
	 */
	static void RequestStudioUpdate(const FStudioDaemonInfo& Info);

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

	/** A live daemon that answered 404 for the Studio extension, so ReadReady passes it over. */
	static int32 ExtensionMissingPid;

private:
	/** Fetch the release manifest and hand a newer Studio than Installed to the updater, or install it. */
	static void CheckForStudioUpdate(const FString& Installed);

	/** Run each command in turn, detached. Refused without install consent (FStudioConsent). Output appends to the Studio log, or stdout goes to StdoutFile when given. */
	static bool SpawnNpx(const TArray<FString>& Commands, FString& OutError, const FString& StdoutFile = FString());
};
