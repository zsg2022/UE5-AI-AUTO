// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOScreenshotCapture.h"
#include "UnrealClient.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

TArray<uint8> FUE5AIAUTOScreenshotCapture::CaptureViewport(const FCaptureSettings& Settings)
{
	TArray<uint8> CompressedData;

	FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTemp, Error, TEXT("[UE5AIAUTO] No active viewport"));
		return CompressedData;
	}

	TArray<FColor> Bitmap;
	FIntRect Rect(0, 0,
		FMath::Min(Settings.Width, Viewport->GetSizeXY().X),
		FMath::Min(Settings.Height, Viewport->GetSizeXY().Y));

	if (!GetViewportScreenShot(Viewport, Bitmap, Rect))
	{
		UE_LOG(LogTemp, Error, TEXT("[UE5AIAUTO] GetViewportScreenShot failed"));
		return CompressedData;
	}

	// Use ImageWrapper for encoding
	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	EImageFormat Format = Settings.bJPEG ? EImageFormat::JPEG : EImageFormat::PNG;
	TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(Format);
	if (!Wrapper.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[UE5AIAUTO] Failed to create ImageWrapper"));
		return CompressedData;
	}

	Wrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor),
		Rect.Width(), Rect.Height(), ERGBFormat::BGRA, 8);

	CompressedData = Wrapper->GetCompressed(Settings.Quality);

	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Screenshot: %dx%d %s, %.1f KB"),
		Rect.Width(), Rect.Height(),
		Settings.bJPEG ? TEXT("JPEG") : TEXT("PNG"),
		CompressedData.Num() / 1024.0f);

	return CompressedData;
}
