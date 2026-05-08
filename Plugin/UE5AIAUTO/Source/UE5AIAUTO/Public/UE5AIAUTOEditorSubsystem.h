// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Containers/Ticker.h"
#include "UE5AIAUTOCommandExecutor.h"
#include "UE5AIAUTOWebSocketClient.h"
#include "UE5AIAUTOEditorSubsystem.generated.h"

/**
 * UE5AIAUTO Editor Subsystem - manages plugin lifecycle.
 *
 * Initialize: creates CommandExecutor, registers handlers, connects WebSocket.
 * Deinitialize: disconnects, cleans up.
 */
UCLASS()
class UE5AIAUTO_API UUE5AIAUTOEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaTime);
	void RegisterCommandHandlers();

	TUniquePtr<FUE5AIAUTOWebSocketClient> WebSocketClient;
	TUniquePtr<FUE5AIAUTOCommandExecutor> CommandExecutor;
	FTSTicker::FDelegateHandle TickDelegateHandle;
};
