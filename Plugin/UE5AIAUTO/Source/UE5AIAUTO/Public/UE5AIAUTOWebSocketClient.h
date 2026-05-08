// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

class FSocket;

DECLARE_MULTICAST_DELEGATE(FUE5AIAUTOOnConnected);
DECLARE_MULTICAST_DELEGATE_OneParam(FUE5AIAUTOOnDisconnected, const FString&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FUE5AIAUTOOnResponse, const FGuid& /*Id*/, const FString& /*Json*/);

class UE5AIAUTO_API FUE5AIAUTOWebSocketClient final : public FRunnable
{
public:
	FUE5AIAUTOWebSocketClient(const FString& Host, int32 Port);
	virtual ~FUE5AIAUTOWebSocketClient();

	void Connect();
	void Disconnect();
	bool IsConnected() const;
	void SendCommand(const FGuid& Id, const FString& JsonMessage);

	FUE5AIAUTOOnConnected OnConnected;
	FUE5AIAUTOOnDisconnected OnDisconnected;
	FUE5AIAUTOOnResponse OnResponse;

protected:
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	FString Host;
	int32 Port;
	void ReadLoop();
	void ReconnectLoop();

	FSocket* Socket = nullptr;
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bShouldRun = false;
	FThreadSafeBool bConnected = false;
	int32 ReconnectAttempt = 0;
	FCriticalSection SendLock;

	FString ReadBuffer;
	static constexpr int32 RecvBufferSize = 65536;

	static constexpr int32 MaxReconnectAttempts = 3;
	static constexpr float ReconnectDelays[3] = {1.0f, 2.0f, 4.0f};
};
