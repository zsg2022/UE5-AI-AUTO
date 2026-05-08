// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOWebSocketClient.h"
#include "Async/Async.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

FUE5AIAUTOWebSocketClient::FUE5AIAUTOWebSocketClient(const FString& InHost, int32 InPort)
	: Host(InHost), Port(InPort) {}

FUE5AIAUTOWebSocketClient::~FUE5AIAUTOWebSocketClient() { Disconnect(); }

void FUE5AIAUTOWebSocketClient::Connect()
{
	if (bConnected) return;
	bShouldRun = true;

	Thread = FRunnableThread::Create(this, TEXT("UE5AIAUTO_TCP"), 0, TPri_Normal);
}

void FUE5AIAUTOWebSocketClient::Disconnect()
{
	bShouldRun = false;
	bConnected = false;

	if (Socket)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}

	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	ReconnectAttempt = 0;
}

bool FUE5AIAUTOWebSocketClient::IsConnected() const { return bConnected && Socket != nullptr; }

void FUE5AIAUTOWebSocketClient::SendCommand(const FGuid& Id, const FString& JsonMessage)
{
	FScopeLock Lock(&SendLock);
	if (!bConnected || !Socket)
	{
		AsyncTask(ENamedThreads::GameThread, [this, Id]()
		{
			FString Err = TEXT("{\"error\":\"Not connected\",\"error_code\":-5}");
			OnResponse.Broadcast(Id, Err);
		});
		return;
	}

	FString Msg = JsonMessage + TEXT("\n");
	TArray<uint8> Data;
	FTCHARToUTF8 Converted(*Msg);
	Data.Append(reinterpret_cast<const uint8*>(Converted.Get()), Converted.Length());
	int32 Sent = 0;
	Socket->Send(Data.GetData(), Data.Num(), Sent);
}

uint32 FUE5AIAUTOWebSocketClient::Run()
{
	while (bShouldRun)
	{
		// Create socket
		Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("UE5AIAUTO"), false);
		if (!Socket)
		{
			FPlatformProcess::Sleep(1.0f);
			continue;
		}

		// Create address (127.0.0.1 always, no DNS needed)
		TSharedRef<FInternetAddr> Addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		bool bValid = false;
		Addr->SetIp(*Host, bValid);
		Addr->SetPort(Port);
		if (!bValid)
		{
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = nullptr;
			FPlatformProcess::Sleep(1.0f);
			continue;
		}

		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] TCP connecting to %s:%d"), *Host, Port);

		if (!Socket->Connect(*Addr))
		{
			UE_LOG(LogTemp, Error, TEXT("[UE5AIAUTO] TCP connect failed: %s:%d"), *Host, Port);
			Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = nullptr;
			ReconnectLoop();
			continue;
		}

		bConnected = true;
		ReconnectAttempt = 0;

		// Send handshake identifying as UE5
		FString Handshake = TEXT("{\"type\":\"ue5\"}\n");
		TArray<uint8> HsData;
		HsData.Append((const uint8*)TCHAR_TO_UTF8(*Handshake), Handshake.Len());
		int32 HsSent = 0;
		Socket->Send(HsData.GetData(), HsData.Num(), HsSent);

		AsyncTask(ENamedThreads::GameThread, [this]() { OnConnected.Broadcast(); });
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] TCP connected"));

		// Read loop
		ReadBuffer.Reset();
		uint8 RecvBuf[RecvBufferSize];
		while (bShouldRun && Socket)
		{
			int32 BytesRead = 0;
			if (!Socket->Recv(RecvBuf, RecvBufferSize, BytesRead, ESocketReceiveFlags::None))
			{
				break; // Connection lost
			}
			if (BytesRead == 0) { FPlatformProcess::Sleep(0.001f); continue; }

			ReadBuffer.AppendChars(UTF8_TO_TCHAR((const char*)RecvBuf), BytesRead);

			// Process complete JSON lines
			int32 NewlineIdx;
			while ((NewlineIdx = ReadBuffer.Find(TEXT("\n"))) != INDEX_NONE)
			{
				FString Line = ReadBuffer.Left(NewlineIdx).TrimEnd();
				ReadBuffer.RightChopInline(NewlineIdx + 1);

				// Extract request ID and dispatch to Game Thread
				FGuid Id;
				FString LineCopy = Line;
				AsyncTask(ENamedThreads::GameThread, [this, LineCopy]()
				{
					// Extract "id" field from JSON (handles both "id":" and "id": " formats)
				FGuid ExtractedId;
				int32 IdKeyPos = LineCopy.Find(TEXT("\"id\""));
				if (IdKeyPos != INDEX_NONE)
				{
					int32 ColonPos = LineCopy.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, IdKeyPos + 4);
					if (ColonPos != INDEX_NONE)
					{
						int32 QuoteStart = LineCopy.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ColonPos + 1);
						if (QuoteStart != INDEX_NONE)
						{
							int32 QuoteEnd = LineCopy.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, QuoteStart + 1);
							if (QuoteEnd != INDEX_NONE)
							{
								FGuid::Parse(LineCopy.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1), ExtractedId);
							}
						}
					}
				}
					OnResponse.Broadcast(ExtractedId, LineCopy);
				});
			}
		}

		// Connection lost
		bConnected = false;
		if (Socket)
		{
			Socket->Close();
			ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
			Socket = nullptr;
		}

		AsyncTask(ENamedThreads::GameThread, [this]() { OnDisconnected.Broadcast(TEXT("Connection lost")); });
		ReconnectLoop();
	}

	return 0;
}

void FUE5AIAUTOWebSocketClient::Stop()
{
	bShouldRun = false;
	bConnected = false;
	if (Socket) { Socket->Close(); Socket = nullptr; }
}

void FUE5AIAUTOWebSocketClient::ReconnectLoop()
{
	while (bShouldRun && !bConnected && ReconnectAttempt < MaxReconnectAttempts)
	{
		float Delay = ReconnectDelays[ReconnectAttempt++];
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Reconnect %d/%d in %.0fs"), ReconnectAttempt, MaxReconnectAttempts, Delay);
		FPlatformProcess::Sleep(Delay);
	}

	// Reset for next connection cycle if max retries reached
	if (ReconnectAttempt >= MaxReconnectAttempts)
	{
		UE_LOG(LogTemp, Warning, TEXT("[UE5AIAUTO] Max reconnect attempts reached"));
	}
}
