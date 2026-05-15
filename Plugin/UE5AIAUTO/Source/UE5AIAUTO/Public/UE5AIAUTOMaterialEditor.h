// Copyright GU5. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

struct FMaterialCreateParams
{
	FString AssetPath;
	FString BaseColorHex = "FFFFFFFF";
	FString MaterialType = "opaque"; // opaque, translucent, masked, emissive, clearcoat, subsurface, additive, modulate
	double Opacity = 1.0;
	double Roughness = 0.5;
	double Metallic = 0.0;
	double Specular = 0.5;
	double EmissiveStrength = 0.0;
	FString EmissiveHex = "000000FF";
};

class UE5AIAUTO_API FUE5AIAUTOMaterialEditor
{
public:
	static TSharedPtr<FJsonObject> CreatePbrMaterial(const FMaterialCreateParams& Params);
};
