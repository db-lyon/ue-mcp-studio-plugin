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

	/** ue-mcp@<the installed bridge's version>, so the server and the bridge move together; ue-mcp@latest without one. */
	static FString PackageSpec();

	/**
	 * Start the daemon with `npx ue-mcp daemon start <uproject>`, detached and
	 * outside any job object the editor would kill it with. Returns false with
	 * OutError when the process could not be created.
	 */
	static bool StartDaemon(FString& OutError);

	/** Install or adopt the bridge with `npx ue-mcp init --yes --json`. Output goes to the Studio log. */
	static bool InstallBridge(FString& OutError);

	/** The UI address, carrying the token the daemon exchanges for a cookie. */
	static FString UiUrl(const FStudioDaemonInfo& Info);

	/** Where Studio writes the output of the processes it starts. */
	static FString LogPath();

private:
	static bool SpawnNpx(const FString& Arguments, FString& OutError);
};
