// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOReflectionScanner.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/UObjectIterator.h"

FUE5AIAUTOReflectionScanner::FScanResult FUE5AIAUTOReflectionScanner::ScanAllModules()
{
	FScanResult Result;
	Result.EngineVersion = GetEngineVersionString();
	Result.ModuleListHash = ComputeModuleListHash();
	const double StartTime = FPlatformTime::Seconds();

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsNative()) continue;
		FString ClassName = Class->GetName();
		FString ModuleName = Class->GetOuterUPackage() ? Class->GetOuterUPackage()->GetName() : TEXT("Unknown");
		Result.ClassNames.Add(ClassName);
		for (TFieldIterator<UFunction> FnIt(Class, EFieldIteratorFlags::IncludeSuper); FnIt; ++FnIt)
		{
			if (UFunction* Fn = *FnIt)
				Result.Functions.Add(ReflectFunction(Fn, ClassName, ModuleName));
		}
	}

	for (TObjectIterator<UEnum> It; It; ++It)
	{
		if (*It) Result.EnumNames.Add((*It)->GetName());
	}

	Result.FunctionCount = Result.Functions.Num();
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Reflection scan: %d functions, %d classes, %.2fs"),
		Result.FunctionCount, Result.ClassNames.Num(), FPlatformTime::Seconds() - StartTime);
	return Result;
}

FUE5AIAUTOReflectionScanner::FScanResult FUE5AIAUTOReflectionScanner::ScanIncremental(const FString& PrevHash)
{
	if (ComputeModuleListHash() == PrevHash)
	{
		FScanResult Empty;
		Empty.EngineVersion = GetEngineVersionString();
		Empty.ModuleListHash = PrevHash;
		return Empty;
	}
	return ScanAllModules();
}

bool FUE5AIAUTOReflectionScanner::SaveCacheToJson(const FScanResult& Result, const FString& FilePath)
{
	TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetStringField(TEXT("engine_version"), Result.EngineVersion);
	Root->SetStringField(TEXT("module_list_hash"), Result.ModuleListHash);
	Root->SetNumberField(TEXT("function_count"), Result.FunctionCount);

	TArray<TSharedPtr<FJsonValue>> FuncArr;
	for (const FReflectedFunction& F : Result.Functions)
	{
		TSharedPtr<FJsonObject> FO = MakeShareable(new FJsonObject());
		FO->SetStringField(TEXT("name"), F.Name);
		FO->SetStringField(TEXT("class_name"), F.ClassName);
		FO->SetStringField(TEXT("module_name"), F.ModuleName);
		FO->SetBoolField(TEXT("is_static"), F.bIsStatic);
		TArray<TSharedPtr<FJsonValue>> PArr;
		for (const FReflectedParameter& P : F.Parameters)
		{
			TSharedPtr<FJsonObject> PO = MakeShareable(new FJsonObject());
			PO->SetStringField(TEXT("name"), P.Name);
			PO->SetStringField(TEXT("type"), P.Type);
			PO->SetBoolField(TEXT("is_return"), P.bIsReturnParam);
			PArr.Add(MakeShareable(new FJsonValueObject(PO)));
		}
		FO->SetArrayField(TEXT("parameters"), PArr);
		FuncArr.Add(MakeShareable(new FJsonValueObject(FO)));
	}
	Root->SetArrayField(TEXT("functions"), FuncArr);

	TArray<TSharedPtr<FJsonValue>> ClsArr;
	for (const FString& N : Result.ClassNames)
		ClsArr.Add(MakeShareable(new FJsonValueString(N)));
	Root->SetArrayField(TEXT("class_names"), ClsArr);

	TArray<TSharedPtr<FJsonValue>> EnmArr;
	for (const FString& N : Result.EnumNames)
		EnmArr.Add(MakeShareable(new FJsonValueString(N)));
	Root->SetArrayField(TEXT("enum_names"), EnmArr);

	FString JsonStr;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&JsonStr);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);
	if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		UE_LOG(LogTemp, Error, TEXT("[UE5AIAUTO] Failed to save cache: %s"), *FilePath);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Cache saved: %s (%.1f KB)"), *FilePath, JsonStr.Len() / 1024.f);
	return true;
}

bool FUE5AIAUTOReflectionScanner::LoadCacheFromJson(FScanResult& Out, const FString& FilePath)
{
	FString JsonStr;
	if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[UE5AIAUTO] Cache not found: %s"), *FilePath);
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid()) return false;

	Root->TryGetStringField(TEXT("engine_version"), Out.EngineVersion);
	Root->TryGetStringField(TEXT("module_list_hash"), Out.ModuleListHash);
	Root->TryGetNumberField(TEXT("function_count"), Out.FunctionCount);

	const TArray<TSharedPtr<FJsonValue>>* FA = nullptr;
	if (Root->TryGetArrayField(TEXT("functions"), FA))
	{
		for (const TSharedPtr<FJsonValue>& V : *FA)
		{
			const TSharedPtr<FJsonObject>* FO = nullptr;
			if (!V->TryGetObject(FO)) continue;
			FReflectedFunction F;
			(*FO)->TryGetStringField(TEXT("name"), F.Name);
			(*FO)->TryGetStringField(TEXT("class_name"), F.ClassName);
			(*FO)->TryGetStringField(TEXT("module_name"), F.ModuleName);
			(*FO)->TryGetBoolField(TEXT("is_static"), F.bIsStatic);
			Out.Functions.Add(F);
		}
	}
	return true;
}

FString FUE5AIAUTOReflectionScanner::ComputeModuleListHash()
{
	// Hash engine version + build timestamp as module list proxy
	return FMD5::HashAnsiString(*FEngineVersion::Current().ToString());
}

FString FUE5AIAUTOReflectionScanner::GetEngineVersionString()
{
	return FEngineVersion::Current().ToString();
}

FReflectedFunction FUE5AIAUTOReflectionScanner::ReflectFunction(
	UFunction* Fn, const FString& ClassName, const FString& ModuleName)
{
	FReflectedFunction RF;
	RF.Name = Fn->GetName();
	RF.ClassName = ClassName;
	RF.ModuleName = ModuleName;
	RF.bIsStatic = Fn->HasAnyFunctionFlags(FUNC_Static);
	for (TFieldIterator<FProperty> It(Fn); It; ++It)
	{
		FProperty* P = *It;
		if (!P) continue;
		FReflectedParameter RP;
		RP.Name = P->GetName();
		RP.Type = P->GetCPPType();
		RP.bIsReturnParam = P->HasAnyPropertyFlags(CPF_ReturnParm);
		RF.Parameters.Add(RP);
	}
	return RF;
}
