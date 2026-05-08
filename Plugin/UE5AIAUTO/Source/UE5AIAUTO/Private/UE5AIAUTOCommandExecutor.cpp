// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOCommandExecutor.h"

DEFINE_LOG_CATEGORY_STATIC(LogUE5AIAUTOExecutor, Log, All);

void FUE5AIAUTOCommandExecutor::RegisterHandler(
	const FString& MethodName, FUE5AIAUTOCommandHandler Handler)
{
	Handlers.Add(MethodName, MoveTemp(Handler));
	UE_LOG(LogUE5AIAUTOExecutor, Log, TEXT("[UE5AIAUTO] Handler 已注册: %s"), *MethodName);
}

void FUE5AIAUTOCommandExecutor::EnqueueCommand(TSharedPtr<FCommandContext> Context)
{
	CommandQueue.Enqueue(Context);
}

void FUE5AIAUTOCommandExecutor::Tick(float DeltaTime)
{
	ProcessQueue();

	// 清理过期命令
	const double Now = FPlatformTime::Seconds();
	ExpiredCommands.RemoveAll([this, Now](const TSharedPtr<FCommandContext>& Ctx)
	{
		if (Now - Ctx->EnqueueTime > CommandTimeoutSeconds)
		{
			UE_LOG(LogUE5AIAUTOExecutor, Warning,
				TEXT("[UE5AIAUTO] 命令超时: %s (%.1fs)"), *Ctx->Method, Now - Ctx->EnqueueTime);
			return true;
		}
		return false;
	});
}

TSharedPtr<FJsonObject> FUE5AIAUTOCommandExecutor::ExecuteImmediate(
	const FString& Method, TSharedPtr<FJsonObject> Params)
{
	const FUE5AIAUTOCommandHandler* Handler = Handlers.Find(Method);
	if (!Handler)
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown method: %s"), *Method));
		Err->SetNumberField(TEXT("error_code"), -2);
		return Err;
	}
	try { return (*Handler)(Params); }
	catch (...)
	{
		TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
		Err->SetStringField(TEXT("error"), TEXT("Handler exception"));
		Err->SetNumberField(TEXT("error_code"), -99);
		return Err;
	}
}

TArray<FString> FUE5AIAUTOCommandExecutor::GetRegisteredMethods() const
{
	TArray<FString> Methods;
	Handlers.GetKeys(Methods);
	return Methods;
}

void FUE5AIAUTOCommandExecutor::ProcessQueue()
{
	TSharedPtr<FCommandContext> Context;
	while (CommandQueue.Dequeue(Context))
	{
		if (!Context.IsValid())
		{
			continue;
		}

		const FUE5AIAUTOCommandHandler* Handler = Handlers.Find(Context->Method);
		if (!Handler)
		{
			UE_LOG(LogUE5AIAUTOExecutor, Warning,
				TEXT("[UE5AIAUTO] 未注册的方法: %s"), *Context->Method);
			continue;
		}

		TSharedPtr<FJsonObject> Result;
		try
		{
			Result = (*Handler)(Context->Params);
		}
		catch (...)
		{
			UE_LOG(LogUE5AIAUTOExecutor, Error,
				TEXT("[UE5AIAUTO] Handler 异常: %s"), *Context->Method);
		}

		// 结果由调用方通过 WebSocket Client 的 PendingRequests 机制返回
		// 这里仅记录执行完成
	}
}
