// Copyright GU5. All Rights Reserved.
#include "UE5AIAUTOMaterialEditor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Factories/MaterialFactoryNew.h"
#include "Engine/Engine.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"

TSharedPtr<FJsonObject> FUE5AIAUTOMaterialEditor::CreatePbrMaterial(const FMaterialCreateParams& Params)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());

	// Parse path
	FString AssetName = FPaths::GetBaseFilename(Params.AssetPath);
	FString FullPath = FPaths::GetPath(Params.AssetPath) / AssetName;
	if (AssetName.IsEmpty()) { Result->SetStringField(TEXT("error"), TEXT("Invalid path")); return Result; }

	// Create package + material
	UPackage* Package = CreatePackage(*FullPath);
	if (!Package) { Result->SetStringField(TEXT("error"), TEXT("Package failed")); return Result; }

	UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
	UMaterial* Mat = (UMaterial*)Factory->FactoryCreateNew(
		UMaterial::StaticClass(), Package, *AssetName,
		RF_Standalone | RF_Public, nullptr, GWarn);
	if (!Mat) { Result->SetStringField(TEXT("error"), TEXT("Material failed")); return Result; }

	// Glass material settings
	Mat->BlendMode = BLEND_Translucent;
	Mat->TwoSided = true;
	Mat->bUseMaterialAttributes = false;

	FLinearColor GlassTint = FLinearColor::White;
	if (!Params.BaseColorHex.IsEmpty())
		GlassTint = FLinearColor::FromSRGBColor(FColor::FromHex(Params.BaseColorHex));

	auto& Exprs = Mat->GetEditorOnlyData()->ExpressionCollection.Expressions;
	int32 X = -600, Y = -400;

	// === BUILD GLASS NODE GRAPH ===
	//
	// Fresnel ──┬──► Lerp(BaseColor, White) → BaseColor
	//           └──► OneMinus → Opacity
	// Roughness ──────► Roughness
	// Specular  ──────► Specular
	// Metallic  ──────► Metallic (always 0 for glass)

	// --- Fresnel ---
	UMaterialExpressionFresnel* FresnelNode = NewObject<UMaterialExpressionFresnel>(Mat);
	FresnelNode->Exponent = 4.0f;  // Sharp glass edge
	FresnelNode->MaterialExpressionEditorX = X;
	FresnelNode->MaterialExpressionEditorY = Y;
	Exprs.Add(FresnelNode);

	// --- BaseColor (glass tint) ---
	UMaterialExpressionConstant3Vector* TintNode = NewObject<UMaterialExpressionConstant3Vector>(Mat);
	TintNode->Constant = GlassTint;
	TintNode->MaterialExpressionEditorX = X;
	TintNode->MaterialExpressionEditorY = Y + 100;
	Exprs.Add(TintNode);

	// --- White (reflection color) ---
	UMaterialExpressionConstant3Vector* WhiteNode = NewObject<UMaterialExpressionConstant3Vector>(Mat);
	WhiteNode->Constant = FLinearColor::White;
	WhiteNode->MaterialExpressionEditorX = X;
	WhiteNode->MaterialExpressionEditorY = Y + 200;
	Exprs.Add(WhiteNode);

	// --- Lerp: tint at center, white at edges ---
	UMaterialExpressionLinearInterpolate* LerpNode = NewObject<UMaterialExpressionLinearInterpolate>(Mat);
	LerpNode->MaterialExpressionEditorX = X + 300;
	LerpNode->MaterialExpressionEditorY = Y;
	Exprs.Add(LerpNode);

	// Fresnel → Lerp.Alpha
	FresnelNode->ConnectExpression(LerpNode->GetInput(2), 0);
	// Tint → Lerp.A (center, no Fresnel)
	TintNode->ConnectExpression(LerpNode->GetInput(0), 0);
	// White → Lerp.B (edge, full Fresnel)
	WhiteNode->ConnectExpression(LerpNode->GetInput(1), 0);
	// Lerp → BaseColor
	Mat->GetEditorOnlyData()->BaseColor.Expression = LerpNode;

	// --- OneMinus(Fresnel) → Opacity ---
	UMaterialExpressionOneMinus* OneMinusNode = NewObject<UMaterialExpressionOneMinus>(Mat);
	OneMinusNode->MaterialExpressionEditorX = X + 300;
	OneMinusNode->MaterialExpressionEditorY = Y + 200;
	Exprs.Add(OneMinusNode);
	FresnelNode->ConnectExpression(OneMinusNode->GetInput(0), 0);
	Mat->GetEditorOnlyData()->Opacity.Expression = OneMinusNode;

	// --- Opacity multiplier ---
	UMaterialExpressionScalarParameter* OpacityParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	OpacityParam->ParameterName = TEXT("Opacity");
	OpacityParam->DefaultValue = Params.Opacity;
	OpacityParam->MaterialExpressionEditorX = X + 300;
	OpacityParam->MaterialExpressionEditorY = Y + 300;
	Exprs.Add(OpacityParam);

	UMaterialExpressionMultiply* OpacityMult = NewObject<UMaterialExpressionMultiply>(Mat);
	OpacityMult->MaterialExpressionEditorX = X + 500;
	OpacityMult->MaterialExpressionEditorY = Y + 200;
	Exprs.Add(OpacityMult);
	OneMinusNode->ConnectExpression(OpacityMult->GetInput(0), 0);
	OpacityParam->ConnectExpression(OpacityMult->GetInput(1), 0);
	Mat->GetEditorOnlyData()->Opacity.Expression = OpacityMult;

	// --- Roughness ---
	UMaterialExpressionScalarParameter* RoughnessParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	RoughnessParam->ParameterName = TEXT("Roughness");
	RoughnessParam->DefaultValue = Params.Roughness;
	RoughnessParam->MaterialExpressionEditorX = X;
	RoughnessParam->MaterialExpressionEditorY = Y + 400;
	Exprs.Add(RoughnessParam);
	Mat->GetEditorOnlyData()->Roughness.Expression = RoughnessParam;

	// --- Specular ---
	UMaterialExpressionScalarParameter* SpecularParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
	SpecularParam->ParameterName = TEXT("Specular");
	SpecularParam->DefaultValue = Params.Specular;
	SpecularParam->MaterialExpressionEditorX = X;
	SpecularParam->MaterialExpressionEditorY = Y + 500;
	Exprs.Add(SpecularParam);
	Mat->GetEditorOnlyData()->Specular.Expression = SpecularParam;

	// --- Metallic = 0 (glass is dielectric) ---
	UMaterialExpressionConstant* MetallicZero = NewObject<UMaterialExpressionConstant>(Mat);
	MetallicZero->R = 0.0f;
	MetallicZero->MaterialExpressionEditorX = X;
	MetallicZero->MaterialExpressionEditorY = Y + 600;
	Exprs.Add(MetallicZero);
	Mat->GetEditorOnlyData()->Metallic.Expression = MetallicZero;

	// Save
	Mat->PreEditChange(nullptr);
	Mat->PostEditChange();
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Mat);

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Standalone | RF_Public;
	FString FilePath = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());
	if (UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs))
	{
		Result->SetStringField(TEXT("status"), TEXT("created"));
		Result->SetStringField(TEXT("asset_path"), FullPath);
		Result->SetNumberField(TEXT("opacity"), Params.Opacity);
		Result->SetNumberField(TEXT("roughness"), Params.Roughness);
		Result->SetStringField(TEXT("material_type"), TEXT("PBR Glass with Fresnel"));
	}
	else
	{
		Result->SetStringField(TEXT("error"), TEXT("Save failed"));
	}
	return Result;
}
