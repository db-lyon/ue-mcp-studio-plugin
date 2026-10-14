// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class AActor;
class FJsonObject;
struct FAssetData;

/** What a quick ask is about, as POST /v1/ext/studio/quick takes it: an actor, asset, location, graph node or graph. */
struct FStudioQuickContext
{
	FString Kind;
	FString Label;
	FString Path;
	FString Class;
	/** For a graph, the cursor's position in it. */
	FString At;
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

	/** Reply markup with its markdown tables as rows of "Header: value" lines, which rich text can lay out; separator rows dropped. */
	static FString TablesAsRows(const FString& Markup);

	/**
	 * A tool call as the Studio panel labels it (client-web's toolLabel): ue-mcp calls by
	 * category.method or search "query", files by path, commands by their first line.
	 * Empty for a call it has no rule for, which keeps the label Studio sent.
	 */
	static FString StepLabel(const FString& Name, const FJsonObject& Input);

	/** The labels of the main thread's tool calls, oldest first, from a GET /sessions/:id body; the calls the pill's steps count. */
	static TArray<FString> MainStepLabels(const FString& SessionBody);
};
