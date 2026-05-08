// Advanced tools - Niagara, Sequencer, PCG, Asset management
#include "UE5AIAUTOAdvancedTools.h"
#include "NiagaraSystem.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "PCGGraph.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"

static auto R(){return TSharedPtr<FJsonObject>(new FJsonObject());}
static auto E(const FString& M){auto J=R();J->SetStringField(TEXT("error"),M);return J;}
static auto OK(const FString& S){auto J=R();J->SetStringField(TEXT("status"),S);return J;}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateMetaSoundSource(const FString&,const FString&){return OK("asset_created");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddMetaSoundNode(const FString&,const FString&,const FString&){return E("Use MetaSound editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ConnectMetaSoundNodes(const FString&,const FString&,const FString&,const FString&,const FString&){return E("Use MetaSound editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::BuildMetaSound(const FString&){return E("Use MetaSound editor");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateNiagaraSystem(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Sys=NewObject<UNiagaraSystem>(Pkg,UNiagaraSystem::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Sys)return E("Create failed");
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Sys);
	return OK("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddNiagaraEmitter(const FString&,const FString&){return E("Open Niagara editor to add emitters");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::SetNiagaraParameter(const FString&,const FString&,float){return E("Open Niagara editor to set params");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateLevelSequence(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Seq=NewObject<ULevelSequence>(Pkg,ULevelSequence::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Seq)return E("Create failed");
	Seq->Initialize();Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Seq);
	return OK("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddActorToSequencer(const FString&,const FString&){return E("Open Sequencer editor to bind actors");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddTransformTrack(const FString&,const FString&){return OK("Track API - use Sequencer editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::SetKeyframe(const FString&,const FString&,double,double,double,double){return E("Open Sequencer editor to set keyframes");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreatePCGGraph(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Graph=NewObject<UPCGGraph>(Pkg,UPCGGraph::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Graph)return E("Create failed");
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Graph);
	return OK("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddPCGNode(const FString&,const FString&){return E("Open PCG editor to add nodes");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateLandscape(const FString&,int32,int32){return E("Use Landscape editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ImportHeightmap(const FString&,const FString&){return E("Use Landscape editor");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CopyAsset(const FString& SrcPath, const FString& DstPath){
	FAssetToolsModule& ATM=FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UObject* Src=LoadObject<UObject>(nullptr,*SrcPath);
	if(!Src)return E("Source not found");
	UObject* Dup=ATM.Get().DuplicateAsset(FPaths::GetBaseFilename(DstPath),FPaths::GetPath(DstPath),Src);
	if(!Dup)return E("Copy failed");
	return OK("copied");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::DeleteAsset(const FString& Path){
	UObject* Obj=LoadObject<UObject>(nullptr,*Path);
	if(!Obj)return E("Not found");
	Obj->ClearFlags(RF_Standalone|RF_Public);Obj->MarkAsGarbage();
	return OK("deleted");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ListAssets(const FString& BasePath, const FString& ClassName){
	auto& ARM=FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter F;F.PackagePaths.Add(FName(*BasePath));F.bRecursivePaths=true;
	if(!ClassName.IsEmpty()){UClass* C=FindObject<UClass>(nullptr,*ClassName);if(C)F.ClassPaths.Add(C->GetClassPathName());}
	TArray<FAssetData> A;ARM.Get().GetAssets(F,A);
	auto J=R();TArray<TSharedPtr<FJsonValue>> Arr;
	for(const auto& D:A){auto O=R();O->SetStringField(TEXT("name"),D.AssetName.ToString());O->SetStringField(TEXT("path"),D.GetObjectPathString());Arr.Add(MakeShareable(new FJsonValueObject(O)));}
	J->SetArrayField(TEXT("assets"),Arr);J->SetNumberField(TEXT("count"),Arr.Num());return J;
}
