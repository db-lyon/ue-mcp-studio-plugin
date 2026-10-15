// Copyright 2026 ue-mcp. All Rights Reserved.

#include "StudioConsent.h"
#include "StudioDaemon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioConsentEvaluateTest, "UE.Studio.Consent.Evaluate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioConsentEvaluateTest::RunTest(const FString&)
{
	const FString Current = FStudioConsent::MakeFileText(TEXT("prompt"));
	const FString Older = FString::Printf(TEXT("{\"version\":%d}"), FStudioConsent::CurrentVersion - 1);
	TestTrue(TEXT("fresh machine asks"), FStudioConsent::Evaluate(false, FString(), false) == EStudioConsent::Ask);
	TestTrue(TEXT("an existing Studio install is adopted"), FStudioConsent::Evaluate(false, FString(), true) == EStudioConsent::Adopt);
	TestTrue(TEXT("a current grant holds"), FStudioConsent::Evaluate(true, Current, false) == EStudioConsent::Granted);
	TestTrue(TEXT("an older grant asks again"), FStudioConsent::Evaluate(true, Older, false) == EStudioConsent::Ask);
	TestTrue(TEXT("an older grant asks again with Studio installed"), FStudioConsent::Evaluate(true, Older, true) == EStudioConsent::Ask);
	TestTrue(TEXT("an unreadable file asks"), FStudioConsent::Evaluate(true, TEXT("not json"), true) == EStudioConsent::Ask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioConsentTargetsTest, "UE.Studio.Consent.Targets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioConsentTargetsTest::RunTest(const FString&)
{
	const TArray<FStudioInstallTarget> Targets = FStudioConsent::Targets();
	TestEqual(TEXT("bridge, npm package, extension, window"), Targets.Num(), 4);
	for (const FStudioInstallTarget& Target : Targets)
	{
		TestFalse(FString::Printf(TEXT("%s has a path"), *Target.What.ToString()), Target.Where.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioConsentOpenStepTest, "UE.Studio.Consent.OpenStep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStudioConsentOpenStepTest::RunTest(const FString&)
{
	// A daemon another MCP client started is live before Studio is installed or allowed.
	TestTrue(TEXT("a live daemon without consent asks"), FStudioDaemon::NextOpenStep(false, false, true) == EStudioOpenStep::AskConsent);
	TestTrue(TEXT("no daemon and no consent asks"), FStudioDaemon::NextOpenStep(false, false, false) == EStudioOpenStep::AskConsent);
	TestTrue(TEXT("a live daemon without the extension installs it"), FStudioDaemon::NextOpenStep(true, false, true) == EStudioOpenStep::Install);
	TestTrue(TEXT("no daemon and no extension installs it"), FStudioDaemon::NextOpenStep(true, false, false) == EStudioOpenStep::Install);
	TestTrue(TEXT("the extension without a daemon starts one"), FStudioDaemon::NextOpenStep(true, true, false) == EStudioOpenStep::Start);
	TestTrue(TEXT("the extension and a live daemon open"), FStudioDaemon::NextOpenStep(true, true, true) == EStudioOpenStep::Open);
	return true;
}

#endif
