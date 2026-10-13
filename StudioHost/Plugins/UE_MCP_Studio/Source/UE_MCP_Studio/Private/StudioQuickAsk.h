// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class AActor;
struct FAssetData;

/** An actor or asset a quick ask is about, as POST /v1/ext/studio/quick takes it. */
struct FStudioQuickContext
{
	FString Kind;
	FString Label;
	FString Path;
	FString Class;
};

/** "Ask Studio", rebindable in Editor Preferences > Keyboard Shortcuts. */
class FStudioQuickAskCommands : public TCommands<FStudioQuickAskCommands>
{
public:
	FStudioQuickAskCommands();
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> AskStudio;
};

/**
 * Ask Studio from anywhere in the editor: a hotkey or Ctrl+Alt+right-click
 * opens a small pill at the cursor, and "Chat about this" on actors and assets
 * opens it with them as context. Sending starts a new Studio session.
 */
class FStudioQuickAsk
{
public:
	static void Startup();
	static void Shutdown();

	/** Open the pill at the cursor, with the context shown as chips and Text in the box. */
	static void Open(const TArray<FStudioQuickContext>& Context, const FString& Text = FString());

	/** Send what the open pill holds, as Enter does. False when no pill is open. */
	static bool SendOpen();

	/** Close the pill, as Esc does. */
	static void Close();

	static TArray<FStudioQuickContext> ContextOfActors(const TArray<AActor*>& Actors);
	static TArray<FStudioQuickContext> ContextOfAssets(const TArray<FAssetData>& Assets);
	static TArray<FStudioQuickContext> ContextOfSelectedActors();

	/** The request body for POST /quick. */
	static FString RequestBody(const FString& Text, const TArray<FStudioQuickContext>& Context);
};
