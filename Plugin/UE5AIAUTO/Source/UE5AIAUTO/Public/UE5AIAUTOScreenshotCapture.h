// Copyright GU5. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 视口截图工具 —— 捕获编辑器活动视口为 JPEG/PNG 编码数据。
 */
class UE5AIAUTO_API FUE5AIAUTOScreenshotCapture
{
public:
	struct FCaptureSettings
	{
		int32 Width = 1920;
		int32 Height = 1080;
		int32 Quality = 85;    // JPEG quality (1-100)
		bool bJPEG = true;     // true=JPEG, false=PNG
	};

	/**
	 * 捕获编辑器活动视口。
	 * @param Settings 捕获参数
	 * @return 编码后的图片数据（JPEG 或 PNG）
	 */
	static TArray<uint8> CaptureViewport(const FCaptureSettings& Settings);
};
