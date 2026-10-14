// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** One thing Studio installs, and where it goes. */
struct FStudioInstallTarget
{
	FText What;
	FString Where;
};

/** What the consent file and the machine say about asking. */
enum class EStudioConsent : uint8
{
	/** The file grants the current consent version. */
	Granted,
	/** No file, but Studio is already installed: record consent and go on. */
	Adopt,
	/** Show the prompt. */
	Ask,
};

/**
 * The user's one-time consent to install. Nothing installs or downloads until it
 * is granted: the bridge plugin, the ue-mcp npm package and the Studio extension
 * and window. Kept per user in ~/.ue-mcp/studio/consent.json; a higher
 * CurrentVersion asks again.
 */
class FStudioConsent
{
public:
	/** Raise when the installed set changes, so everyone is asked again. */
	static constexpr int32 CurrentVersion = 1;

	static FString FilePath();

	/** Consent granted, or adopted from an existing Studio install. */
	static bool IsGranted();

	/** Record the user's Allow. */
	static bool Grant();

	/** Everything Allow covers, with concrete paths for this project and user. */
	static TArray<FStudioInstallTarget> Targets();

	/** The decision for a consent file's text (empty when there is none) and whether Studio is installed. */
	static EStudioConsent Evaluate(bool bFileExists, const FString& FileText, bool bStudioInstalled);

	/** The consent file's text for a grant. Source is "prompt" or "existing-install". */
	static FString MakeFileText(const TCHAR* Source);
};
