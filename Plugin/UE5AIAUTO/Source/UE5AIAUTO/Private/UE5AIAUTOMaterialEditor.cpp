// Universal material creator - builds node graph from user parameters
#include "UE5AIAUTOMaterialEditor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Factories/MaterialFactoryNew.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"

TSharedPtr<FJsonObject> FUE5AIAUTOMaterialEditor::CreatePbrMaterial(const FMaterialCreateParams& P)
{
	auto R=TSharedPtr<FJsonObject>(new FJsonObject());

	FString Name=FPaths::GetBaseFilename(P.AssetPath);
	FString Folder=FPaths::GetPath(P.AssetPath);
	UPackage* Pkg=CreatePackage(*(Folder/Name));
	if(!Pkg){R->SetStringField(TEXT("error"),TEXT("Package failed"));return R;}

	UMaterialFactoryNew* Fac=NewObject<UMaterialFactoryNew>();
	auto* M=Cast<UMaterial>(Fac->FactoryCreateNew(UMaterial::StaticClass(),Pkg,*Name,RF_Standalone|RF_Public,nullptr,GWarn));
	if(!M){R->SetStringField(TEXT("error"),TEXT("Create failed"));return R;}

	// --- Set blend mode ---
	if(P.MaterialType==TEXT("translucent")){M->BlendMode=BLEND_Translucent;M->TwoSided=true;}
	else if(P.MaterialType==TEXT("masked")){M->BlendMode=BLEND_Masked;}
	else if(P.MaterialType==TEXT("additive")){M->BlendMode=BLEND_Additive;}
	else if(P.MaterialType==TEXT("modulate")){M->BlendMode=BLEND_Modulate;}
	else{M->BlendMode=BLEND_Opaque;} // default

	if(P.MaterialType==TEXT("clearcoat"))M->SetShadingModel(MSM_ClearCoat);
	else if(P.MaterialType==TEXT("subsurface"))M->SetShadingModel(MSM_Subsurface);

	auto& Exprs=M->GetEditorOnlyData()->ExpressionCollection.Expressions;
	int32 X=-400;

	// --- Base Color ---
	FLinearColor BC=FLinearColor::FromSRGBColor(FColor::FromHex(P.BaseColorHex));
	auto* BaseNode=NewObject<UMaterialExpressionConstant3Vector>(M);
	BaseNode->Constant=BC;BaseNode->MaterialExpressionEditorX=X;BaseNode->MaterialExpressionEditorY=0;
	Exprs.Add(BaseNode);M->GetEditorOnlyData()->BaseColor.Expression=BaseNode;

	// --- Roughness ---
	auto* RoughNode=NewObject<UMaterialExpressionScalarParameter>(M);
	RoughNode->ParameterName=TEXT("Roughness");RoughNode->DefaultValue=P.Roughness;
	RoughNode->MaterialExpressionEditorX=X;RoughNode->MaterialExpressionEditorY=120;
	Exprs.Add(RoughNode);M->GetEditorOnlyData()->Roughness.Expression=RoughNode;

	// --- Metallic ---
	auto* MetalNode=NewObject<UMaterialExpressionScalarParameter>(M);
	MetalNode->ParameterName=TEXT("Metallic");MetalNode->DefaultValue=P.Metallic;
	MetalNode->MaterialExpressionEditorX=X;MetalNode->MaterialExpressionEditorY=200;
	Exprs.Add(MetalNode);M->GetEditorOnlyData()->Metallic.Expression=MetalNode;

	// --- Specular ---
	auto* SpecNode=NewObject<UMaterialExpressionScalarParameter>(M);
	SpecNode->ParameterName=TEXT("Specular");SpecNode->DefaultValue=P.Specular;
	SpecNode->MaterialExpressionEditorX=X;SpecNode->MaterialExpressionEditorY=280;
	Exprs.Add(SpecNode);M->GetEditorOnlyData()->Specular.Expression=SpecNode;

	// --- Emissive (if strength > 0) ---
	if(P.EmissiveStrength>0.001){
		FLinearColor EC=FLinearColor::FromSRGBColor(FColor::FromHex(P.EmissiveHex));
		auto* EmNode=NewObject<UMaterialExpressionConstant3Vector>(M);
		EmNode->Constant=EC;EmNode->MaterialExpressionEditorX=X;EmNode->MaterialExpressionEditorY=360;
		Exprs.Add(EmNode);
		auto* EmMul=NewObject<UMaterialExpressionMultiply>(M);
		EmMul->MaterialExpressionEditorX=X+200;EmMul->MaterialExpressionEditorY=360;
		Exprs.Add(EmMul);
		auto* EmStr=NewObject<UMaterialExpressionScalarParameter>(M);
		EmStr->ParameterName=TEXT("EmissiveStrength");EmStr->DefaultValue=P.EmissiveStrength;
		EmStr->MaterialExpressionEditorX=X;EmStr->MaterialExpressionEditorY=440;
		Exprs.Add(EmStr);
		EmNode->ConnectExpression(EmMul->GetInput(0),0);
		EmStr->ConnectExpression(EmMul->GetInput(1),0);
		M->GetEditorOnlyData()->EmissiveColor.Expression=EmMul;
	}

	// --- Opacity (for translucent) ---
	if(P.MaterialType==TEXT("translucent")){
		auto* OpNode=NewObject<UMaterialExpressionScalarParameter>(M);
		OpNode->ParameterName=TEXT("Opacity");OpNode->DefaultValue=P.Opacity;
		OpNode->MaterialExpressionEditorX=X;OpNode->MaterialExpressionEditorY=520;
		Exprs.Add(OpNode);M->GetEditorOnlyData()->Opacity.Expression=OpNode;
	}

	// --- Opacity Mask (for masked) ---
	if(P.MaterialType==TEXT("masked")){
		auto* OmNode=NewObject<UMaterialExpressionScalarParameter>(M);
		OmNode->ParameterName=TEXT("OpacityMask");OmNode->DefaultValue=0.5;
		OmNode->MaterialExpressionEditorX=X;OmNode->MaterialExpressionEditorY=520;
		Exprs.Add(OmNode);M->GetEditorOnlyData()->OpacityMask.Expression=OmNode;
	}

	M->PreEditChange(nullptr);M->PostEditChange();
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(M);

	FSavePackageArgs SA;SA.TopLevelFlags=RF_Standalone|RF_Public;
	UPackage::SavePackage(Pkg,M,*FPackageName::LongPackageNameToFilename(Folder/Name,FPackageName::GetAssetPackageExtension()),SA);

	R->SetStringField(TEXT("status"),TEXT("created"));
	R->SetStringField(TEXT("type"),P.MaterialType);
	R->SetStringField(TEXT("path"),Folder/Name);
	return R;
}
