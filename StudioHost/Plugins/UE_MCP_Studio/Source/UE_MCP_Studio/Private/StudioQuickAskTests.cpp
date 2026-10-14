// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioQuickAsk.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioQuickAskTablesTest, "UE.Studio.QuickAsk.TablesAsRows",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioQuickAskTablesTest::RunTest(const FString&)
{
	const FString Bullet = TEXT("•  ");
	TestEqual(TEXT("no table"), FStudioQuickAsk::TablesAsRows(TEXT("a | b\nplain")), FString(TEXT("a | b\nplain")));
	TestEqual(TEXT("table"),
		FStudioQuickAsk::TablesAsRows(TEXT("Lights:\n\n| Name | Intensity |\n|:---|---:|\n| Sun | 10 lux |\n| <c>Fill</> | |\n\nDone.")),
		FString(TEXT("Lights:\n\n")) + Bullet + TEXT("<b>Name</>: Sun\n    <b>Intensity</>: 10 lux\n") + Bullet + TEXT("<b>Name</>: <c>Fill</>\n\nDone."));
	TestEqual(TEXT("no outer pipes, escaped pipe, marked-up header"),
		FStudioQuickAsk::TablesAsRows(TEXT("<b>Key</> | Value\n--- | ---\nA | x \\| y")),
		Bullet + TEXT("<b>Key</>: A\n    <b>Value</>: x | y"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioQuickAskStepLabelsTest, "UE.Studio.QuickAsk.StepLabels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioQuickAskStepLabelsTest::RunTest(const FString&)
{
	auto Label = [](const FString& Name, const FString& Input)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Input), Object) || !Object.IsValid())
		{
			Object = MakeShared<FJsonObject>();
		}
		return FStudioQuickAsk::StepLabel(Name, *Object);
	};
	TestEqual(TEXT("ue-mcp call"), Label(TEXT("ue_mcp"), TEXT("{\"action\":\"call\",\"category\":\"level\",\"method\":\"get_component_tree\",\"args\":{}}")), FString(TEXT("level.get_component_tree")));
	TestEqual(TEXT("ue-mcp search"), Label(TEXT("ue_mcp"), TEXT("{\"action\":\"search\",\"query\":\"material inputs\"}")), FString(TEXT("search \"material inputs\"")));
	TestEqual(TEXT("ue-mcp describe"), Label(TEXT("ue_mcp"), TEXT("{\"action\":\"describe\",\"category\":\"pcg\"}")), FString(TEXT("describe pcg")));
	TestEqual(TEXT("ue-mcp describe one action"), Label(TEXT("ue_mcp"), TEXT("{\"action\":\"describe\",\"category\":\"pcg\",\"method\":\"export_graph\"}")), FString(TEXT("describe pcg.export_graph")));
	TestEqual(TEXT("shell"), Label(TEXT("shell"), TEXT("{\"command\":\"git status\\ngit log\"}")), FString(TEXT("git status (+1 lines)")));
	TestEqual(TEXT("read part of a file"), Label(TEXT("read_file"), TEXT("{\"path\":\"a.txt\",\"limit\":20}")), FString(TEXT("a.txt (from line 1)")));
	TestEqual(TEXT("grep"), Label(TEXT("grep"), TEXT("{\"pattern\":\"Tick\",\"glob\":\"*.cpp\",\"path\":\"Source\"}")), FString(TEXT("/Tick/ *.cpp in Source")));
	TestEqual(TEXT("web search"), Label(TEXT("web_search"), TEXT("{\"query\":\"nanite\"}")), FString(TEXT("\"nanite\"")));
	TestEqual(TEXT("no rule keeps Studio's label"), Label(TEXT("agent"), TEXT("{\"description\":\"x\"}")), FString());

	const TArray<FString> Main = FStudioQuickAsk::MainStepLabels(TEXT(
		"{\"id\":\"s\",\"transcript\":["
		"{\"kind\":\"user\",\"text\":\"why\",\"at\":\"t\"},"
		"{\"kind\":\"tool\",\"call\":{\"id\":\"1\",\"name\":\"ue_mcp\",\"input\":{\"action\":\"search\",\"query\":\"emissive\"},\"outcome\":\"done\"}},"
		"{\"kind\":\"tool\",\"parent\":\"9\",\"call\":{\"id\":\"2\",\"name\":\"ue_mcp\",\"input\":{\"action\":\"call\",\"category\":\"asset\",\"method\":\"list\"},\"outcome\":\"done\"}},"
		"{\"kind\":\"tool\",\"call\":{\"id\":\"3\",\"name\":\"ue_mcp\",\"input\":{\"action\":\"call\",\"category\":\"material\",\"method\":\"export_graph\"},\"outcome\":\"pending\"}},"
		"{\"kind\":\"assistant\",\"text\":\"done\",\"at\":\"t\"}]}"));
	TestEqual(TEXT("main thread's calls only"), FString::Join(Main, TEXT(" | ")), FString(TEXT("search \"emissive\" | material.export_graph")));
	TestEqual(TEXT("not a session"), FStudioQuickAsk::MainStepLabels(TEXT("nope")).Num(), 0);
	return true;
}

#endif
