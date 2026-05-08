// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UE5AIAUTOTypes.h"

/**
 * 反射扫描引擎 —— 扫描 UE5 反射系统，生成 JSON 缓存。
 *
 * 用法：
 *   FUE5AIAUTOReflectionScanner::FScanResult Result =
 *       FUE5AIAUTOReflectionScanner::ScanAllModules();
 *   FUE5AIAUTOReflectionScanner::SaveCacheToJson(Result, FilePath);
 */
class UE5AIAUTO_API FUE5AIAUTOReflectionScanner
{
public:
	struct FScanResult
	{
		TArray<FReflectedFunction> Functions;
		TArray<FString> ClassNames;
		TArray<FString> EnumNames;
		FString EngineVersion;
		FString ModuleListHash;
		int32 FunctionCount = 0;
	};

	/** 全量扫描所有已加载模块的反射数据 */
	static FScanResult ScanAllModules();

	/** 增量扫描：仅当 ModuleHash 变化时扫描 */
	static FScanResult ScanIncremental(const FString& PreviousModuleHash);

	/** 将扫描结果序列化为 JSON 并写入文件 */
	static bool SaveCacheToJson(const FScanResult& Result, const FString& FilePath);

	/** 从 JSON 文件加载缓存 */
	static bool LoadCacheFromJson(FScanResult& OutResult, const FString& FilePath);

private:
	/** 计算当前已加载模块列表的哈希 */
	static FString ComputeModuleListHash();

	/** 获取引擎版本字符串 */
	static FString GetEngineVersionString();

	/** 从 UFunction 提取反射元数据 */
	static FReflectedFunction ReflectFunction(UFunction* Function, const FString& ClassName, const FString& ModuleName);

	/** 所有扫描的枚举值序列化为 JSON */
	static TSharedPtr<FJsonObject> ScanResultToJson(const FScanResult& Result);
	static bool JsonToScanResult(const TSharedPtr<FJsonObject>& Json, FScanResult& OutResult);
};
