// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UE5AIAUTOTypes.h"

/** 命令处理器签名：接收参数 JSON，返回结果 JSON */
using FUE5AIAUTOCommandHandler = TFunction<TSharedPtr<FJsonObject>(TSharedPtr<FJsonObject> Params)>;

/**
 * 命令执行器 —— 将 WebSocket 命令投递到 Game Thread 执行。
 *
 * 线程模型:
 *   WebSocket Thread → EnqueueCommand() [Mpsc 队列]
 *   Game Thread     → Tick() → ProcessQueue() → 执行 handler
 */
class UE5AIAUTO_API FUE5AIAUTOCommandExecutor
{
public:
	FUE5AIAUTOCommandExecutor() = default;
	~FUE5AIAUTOCommandExecutor() = default;

	/** 注册命令处理器 */
	void RegisterHandler(const FString& MethodName, FUE5AIAUTOCommandHandler Handler);

	/** 从任意线程投递命令（线程安全） */
	void EnqueueCommand(TSharedPtr<FCommandContext> Context);

	/** 每帧调用（Game Thread），处理队列中的命令 */
	void Tick(float DeltaTime);

	/** 获取已注册的 handler 名称列表 */
	TArray<FString> GetRegisteredMethods() const;

	/** 同步执行命令（须在 Game Thread 调用） */
	TSharedPtr<FJsonObject> ExecuteImmediate(const FString& Method, TSharedPtr<FJsonObject> Params);

private:
	void ProcessQueue();

	TMap<FString, FUE5AIAUTOCommandHandler> Handlers;
	TQueue<TSharedPtr<FCommandContext>, EQueueMode::Mpsc> CommandQueue;
	TArray<TSharedPtr<FCommandContext>> ExpiredCommands;

	static constexpr float CommandTimeoutSeconds = 30.0f;
};
