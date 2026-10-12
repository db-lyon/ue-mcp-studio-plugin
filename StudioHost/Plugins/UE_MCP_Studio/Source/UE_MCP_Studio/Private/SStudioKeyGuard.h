// Copyright 2026 ue-mcp. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Holds a browser view. The browser hands back every key the page did not
 * cancel, typing included, and those would reach the level editor's shortcuts
 * (W moves, G hides, Delete deletes). Every key stops here instead.
 */
class SStudioKeyGuard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStudioKeyGuard) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ChildSlot[InArgs._Content.Widget];
	}

	virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
	virtual FReply OnKeyUp(const FGeometry&, const FKeyEvent&) override { return FReply::Handled(); }
	virtual FReply OnKeyChar(const FGeometry&, const FCharacterEvent&) override { return FReply::Handled(); }
};
