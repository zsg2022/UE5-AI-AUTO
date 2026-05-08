// Copyright GU5. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

struct FMaterialCreateParams
{
	FString AssetPath;        // "/Game/材质/M_Glass"
	FString BaseColorHex;     // "80D0FF" (light blue tint)
	double Opacity = 0.3;
	double Roughness = 0.1;
	double Metallic = 0.0;
	double Specular = 0.5;
};

class UE5AIAUTO_API FUE5AIAUTOMaterialEditor
{
public:
	static TSharedPtr<FJsonObject> CreatePbrMaterial(const FMaterialCreateParams& Params);
};
