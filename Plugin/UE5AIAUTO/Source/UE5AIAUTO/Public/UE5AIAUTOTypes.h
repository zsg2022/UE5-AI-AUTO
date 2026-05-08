// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * UE5AIAUTO 共享数据类型 —— 跨模块使用的 DTO。
 * 包含：反射数据、命令上下文、错误码。
 */

// ═══════════════════════════════════════════
// 错误码
// ═══════════════════════════════════════════

enum class EUE5AIAUTOErrorCode : int32
{
	OK = 0,
	INVALID_ARG = -1,
	NOT_FOUND = -2,
	TYPE_MISMATCH = -3,
	TIMEOUT = -4,
	DISCONNECTED = -5,
	INTERNAL = -99
};


// ═══════════════════════════════════════════
// 反射类型
// ═══════════════════════════════════════════

struct UE5AIAUTO_API FReflectedProperty
{
	FString Name;
	FString Type;
	FString MetaData;  // BlueprintReadWrite, EditAnywhere, etc.
};

struct UE5AIAUTO_API FReflectedParameter
{
	FString Name;
	FString Type;
	bool bIsReturnParam = false;
};

struct UE5AIAUTO_API FReflectedFunction
{
	FString Name;
	FString ClassName;
	FString ModuleName;
	TArray<FReflectedParameter> Parameters;
	bool bIsStatic = false;
};


// ═══════════════════════════════════════════
// 命令类型（跨线程 DTO）
// ═══════════════════════════════════════════

struct FCommandContext
{
	FGuid Id;
	FString Method;
	TSharedPtr<FJsonObject> Params;
	double EnqueueTime = 0.0;  // FPlatformTime::Seconds()

	FCommandContext() = default;
	FCommandContext(const FGuid& InId, const FString& InMethod,
		TSharedPtr<FJsonObject> InParams)
		: Id(InId), Method(InMethod), Params(InParams)
		, EnqueueTime(FPlatformTime::Seconds()) {}
};

struct FCommandResult
{
	FGuid Id;
	TSharedPtr<FJsonObject> Result;
	FString Error;
	int32 ErrorCode = 0;  // EUE5AIAUTOErrorCode
	TArray<uint8> BinaryAttachment;

	bool IsSuccess() const { return Error.IsEmpty(); }
};
