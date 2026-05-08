// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTO.h"
#include "UE5AIAUTOEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "FUE5AIAUTOModule"

void FUE5AIAUTOModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Module StartupModule"));
}

void FUE5AIAUTOModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Module ShutdownModule"));
}

FUE5AIAUTOModule& FUE5AIAUTOModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUE5AIAUTOModule>("UE5AIAUTO");
}

bool FUE5AIAUTOModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("UE5AIAUTO");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5AIAUTOModule, UE5AIAUTO)
