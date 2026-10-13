// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioQuickAsk.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioQuickAskChordTest, "UE.Studio.QuickAsk.DefaultChordIsFree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioQuickAskChordTest::RunTest(const FString&)
{
	const TSharedPtr<FUICommandInfo> Ask = FStudioQuickAskCommands::Get().AskStudio;
	TestTrue(TEXT("Ask Studio is registered"), Ask.IsValid());
	if (!Ask.IsValid())
	{
		return false;
	}
	const FInputChord Chord = Ask->GetDefaultChord(EMultipleKeyBindingIndex::Primary);
	TestEqual(TEXT("default chord"), Chord, FInputChord(EModifierKey::Control | EModifierKey::Alt, EKeys::SpaceBar));
	TArray<TSharedPtr<FBindingContext>> Contexts;
	FInputBindingManager::Get().GetKnownInputContexts(Contexts);
	int32 Checked = 0;
	for (const TSharedPtr<FBindingContext>& Context : Contexts)
	{
		const TSharedPtr<FUICommandInfo> Other = FInputBindingManager::Get().FindCommandInContext(Context->GetContextName(), Chord, true);
		if (Other.IsValid() && Other != Ask)
		{
			AddError(FString::Printf(TEXT("%s is also the default of %s.%s"), *Chord.GetInputText().ToString(), *Context->GetContextName().ToString(), *Other->GetCommandName().ToString()));
		}
		++Checked;
	}
	AddInfo(FString::Printf(TEXT("checked %d input contexts"), Checked));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioQuickAskBodyTest, "UE.Studio.QuickAsk.RequestBody",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioQuickAskBodyTest::RunTest(const FString&)
{
	TestEqual(TEXT("text only"), FStudioQuickAsk::RequestBody(TEXT("hi \"there\""), {}), FString(TEXT("{\"text\":\"hi \\\"there\\\"\"}")));
	const TArray<FStudioQuickContext> Context = { { TEXT("actor"), TEXT("Terrain"), TEXT("/Game/Maps/L.L:PersistentLevel.Terrain"), TEXT("Landscape") }, { TEXT("asset"), TEXT("M_Rock"), TEXT("/Game/M_Rock.M_Rock"), FString() } };
	TestEqual(TEXT("with context"), FStudioQuickAsk::RequestBody(TEXT("why"), Context),
		FString(TEXT("{\"text\":\"why\",\"context\":[{\"kind\":\"actor\",\"label\":\"Terrain\",\"path\":\"/Game/Maps/L.L:PersistentLevel.Terrain\",\"class\":\"Landscape\"},{\"kind\":\"asset\",\"label\":\"M_Rock\",\"path\":\"/Game/M_Rock.M_Rock\"}]}")));
	return true;
}

#endif
