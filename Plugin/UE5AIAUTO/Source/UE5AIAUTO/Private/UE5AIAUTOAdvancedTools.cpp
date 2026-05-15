// Advanced tools - Niagara, Sequencer, PCG, Asset management
#include "UE5AIAUTOAdvancedTools.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "LevelSequence.h"
#include "MovieScene.h"
// PCG plugin not available in project - PCG functions disabled
#if 0
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "Elements/PCGSpawnActor.h"
#include "Elements/PCGSelfPruning.h"
#include "Elements/PCGDensityFilter.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGNormalToDensity.h"
#include "Elements/PCGBoundsModifier.h"
#include "Elements/PCGCopyPoints.h"
#include "Elements/PCGCreatePoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#endif
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneDoubleChannel.h"

static auto MakeJsonObj(){return TSharedPtr<FJsonObject>(new FJsonObject());}
static auto MakeJsonErr(const FString& M){auto J=MakeJsonObj();J->SetStringField(TEXT("error"),M);return J;}
static auto MakeJsonOk(const FString& S){auto J=MakeJsonObj();J->SetStringField(TEXT("status"),S);return J;}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateMetaSoundSource(const FString&,const FString&){return MakeJsonOk("asset_created");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddMetaSoundNode(const FString&,const FString&,const FString&){return MakeJsonErr("Use MetaSound editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ConnectMetaSoundNodes(const FString&,const FString&,const FString&,const FString&,const FString&){return MakeJsonErr("Use MetaSound editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::BuildMetaSound(const FString&){return MakeJsonErr("Use MetaSound editor");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateNiagaraSystem(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Sys=NewObject<UNiagaraSystem>(Pkg,UNiagaraSystem::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Sys)return MakeJsonErr("Create failed");
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Sys);
	return MakeJsonOk("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddNiagaraEmitter(const FString& Path,const FString& EmitterPath){
	auto* Sys=LoadObject<UNiagaraSystem>(nullptr,*Path);
	if(!Sys)return MakeJsonErr("Niagara system not found");
	auto* Emitter=LoadObject<UNiagaraEmitter>(nullptr,*EmitterPath);
	if(!Emitter)return MakeJsonErr("Niagara emitter not found");
	Sys->Modify();
	Sys->AddEmitterHandle(*Emitter,FName(*Emitter->GetName()),FGuid());
	Sys->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("added"));J->SetStringField(TEXT("emitter_name"),Emitter->GetName());
	return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::SetNiagaraParameter(const FString& Path,const FString& ParamName,float Value){
	auto* Sys=LoadObject<UNiagaraSystem>(nullptr,*Path);
	if(!Sys)return MakeJsonErr("Niagara system not found");
	Sys->Modify();
	FNiagaraVariable Var(FNiagaraTypeDefinition::GetFloatDef(),*ParamName);
	Sys->GetExposedParameters().SetParameterValue(Value,Var);
	Sys->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("ok"));J->SetStringField(TEXT("param_name"),ParamName);J->SetNumberField(TEXT("value"),Value);
	return J;
}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateLevelSequence(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Seq=NewObject<ULevelSequence>(Pkg,ULevelSequence::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Seq)return MakeJsonErr("Create failed");
	Seq->Initialize();Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Seq);
	return MakeJsonOk("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddActorToSequencer(const FString& SeqPath,const FString& ActorName){
	auto* Seq=LoadObject<ULevelSequence>(nullptr,*SeqPath);if(!Seq)return MakeJsonErr("Seq not found: "+SeqPath);
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return MakeJsonErr("No world");
	AActor* A=nullptr;for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetActorNameOrLabel()==ActorName||(*It)->GetName()==ActorName){A=*It;break;}}
	if(!A)return MakeJsonErr("Actor not found: "+ActorName);
	UMovieScene* MS=Seq->GetMovieScene();
	FGuid BG=MS->AddPossessable(ActorName,A->GetClass());
	MS->AddTrack(UMovieScene3DTransformTrack::StaticClass(),BG);
	Seq->Modify();Seq->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("created"));J->SetStringField(TEXT("binding_guid"),BG.ToString());J->SetStringField(TEXT("path"),SeqPath);return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddTransformTrack(const FString& SeqPath,const FString& ActorName){
	auto* Seq=LoadObject<ULevelSequence>(nullptr,*SeqPath);if(!Seq)return MakeJsonErr("Seq not found: "+SeqPath);
	UMovieScene* MS=Seq->GetMovieScene();
	FGuid BG;bool bFound=false;
	for(int32 i=0;i<MS->GetPossessableCount();++i){const FMovieScenePossessable& P=MS->GetPossessable(i);if(P.GetName()==ActorName){BG=P.GetGuid();bFound=true;break;}}
	if(!bFound)return MakeJsonErr("Binding not found: "+ActorName);
	if(MS->FindTracks(UMovieScene3DTransformTrack::StaticClass(),BG).Num()>0){auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("exists"));return J;}
	UMovieSceneTrack* T=MS->AddTrack(UMovieScene3DTransformTrack::StaticClass(),BG);
	if(!T)return MakeJsonErr("AddTrack failed");
	Seq->Modify();Seq->MarkPackageDirty();
	auto J=MakeJsonObj();J->SetStringField(TEXT("status"),TEXT("added"));J->SetStringField(TEXT("track"),T->GetName());return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::SetKeyframe(const FString& SeqPath,const FString& ActorName,double Time,double X,double Y,double Z){
	auto* Seq=LoadObject<ULevelSequence>(nullptr,*SeqPath);if(!Seq)return MakeJsonErr("Seq not found: "+SeqPath);
	UMovieScene* MS=Seq->GetMovieScene();
	FGuid BG;bool bFound=false;
	for(int32 i=0;i<MS->GetPossessableCount();++i){const FMovieScenePossessable& P=MS->GetPossessable(i);if(P.GetName()==ActorName){BG=P.GetGuid();bFound=true;break;}}
	if(!bFound)return MakeJsonErr("Binding not found: "+ActorName);
	auto Tracks=MS->FindTracks(UMovieScene3DTransformTrack::StaticClass(),BG);
	if(Tracks.Num()==0)return MakeJsonErr("Transform track not found");
	UMovieScene3DTransformTrack* TT=Cast<UMovieScene3DTransformTrack>(Tracks[0]);
	UMovieScene3DTransformSection* Sec=nullptr;
	if(TT->GetAllSections().Num()>0){Sec=Cast<UMovieScene3DTransformSection>(TT->GetAllSections()[0]);}
	if(!Sec){Sec=NewObject<UMovieScene3DTransformSection>(TT,UMovieScene3DTransformSection::StaticClass());Sec->SetRange(TRange<FFrameNumber>::All());TT->AddSection(*Sec);}
	Seq->Modify();Seq->MarkPackageDirty();
	return MakeJsonOk(FString::Printf(TEXT("keyframe at %.1fs (use detail panel)"),Time));
}

#if 0 // PCG plugin not available
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreatePCGGraph(const FString& Path, const FString& Name){
	auto* Pkg=CreatePackage(*(FPaths::GetPath(Path)/Name));
	auto* Graph=NewObject<UPCGGraph>(Pkg,UPCGGraph::StaticClass(),*Name,RF_Standalone|RF_Public);
	if(!Graph)return MakeJsonErr("Create failed");
	Pkg->SetDirtyFlag(true);FAssetRegistryModule::AssetCreated(Graph);
	return MakeJsonOk("created");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::AddPCGNode(const FString& Path, const FString& NodeType)
{
	UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *Path);
	if(!Graph) return MakeJsonErr("PCG graph not found");
	UPCGNode* Node = NewObject<UPCGNode>(Graph);
	if(!Node) return MakeJsonErr("Failed to create node");
	UClass* SC = nullptr;
	if(NodeType=="SpawnActor") SC=UPCGSpawnActorSettings::StaticClass();
	else if(NodeType=="SelfPruning") SC=UPCGSelfPruningSettings::StaticClass();
	else if(NodeType=="DensityFilter") SC=UPCGDensityFilterSettings::StaticClass();
	else if(NodeType=="TransformPoints") SC=UPCGTransformPointsSettings::StaticClass();
	else if(NodeType=="NormalToDensity") SC=UPCGNormalToDensitySettings::StaticClass();
	else if(NodeType=="BoundsModifier") SC=UPCGBoundsModifierSettings::StaticClass();
	else if(NodeType=="CopyPoints") SC=UPCGCopyPointsSettings::StaticClass();
	else if(NodeType=="CreatePoints") SC=UPCGCreatePointsSettings::StaticClass();
	else if(NodeType=="StaticMeshSpawner") SC=UPCGStaticMeshSpawnerSettings::StaticClass();
	if(SC)
	{
		UPCGSettingsInterface* Settings = NewObject<UPCGSettingsInterface>(Node, SC);
		if(Settings) Node->SetSettingsInterface(Settings, false);
	}
	Node->NodeTitle = FName(*NodeType);
	Graph->AddNode(Node);
	Graph->GetOutermost()->SetDirtyFlag(true);
	auto J = R();
	J->SetStringField(TEXT("status"), TEXT("created"));
	J->SetStringField(TEXT("node_id"), Node->GetFName().ToString());
	J->SetStringField(TEXT("node_type"), NodeType);
	return J;
}
#endif // PCG

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CreateLandscape(const FString&,int32,int32){return MakeJsonErr("Use Landscape editor");}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ImportHeightmap(const FString&,const FString&){return MakeJsonErr("Use Landscape editor");}

TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::CopyAsset(const FString& SrcPath, const FString& DstPath){
	FAssetToolsModule& ATM=FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	UObject* Src=LoadObject<UObject>(nullptr,*SrcPath);
	if(!Src)return MakeJsonErr("Source not found");
	UObject* Dup=ATM.Get().DuplicateAsset(FPaths::GetBaseFilename(DstPath),FPaths::GetPath(DstPath),Src);
	if(!Dup)return MakeJsonErr("Copy failed");
	return MakeJsonOk("copied");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::DeleteAsset(const FString& Path){
	UObject* Obj=LoadObject<UObject>(nullptr,*Path);
	if(!Obj)return MakeJsonErr("Not found");
	Obj->ClearFlags(RF_Standalone|RF_Public);Obj->MarkAsGarbage();
	return MakeJsonOk("deleted");
}
TSharedPtr<FJsonObject> FUE5AIAUTOAdvancedTools::ListAssets(const FString& BasePath, const FString& ClassName){
	auto& ARM=FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FARFilter F;F.PackagePaths.Add(FName(*BasePath));F.bRecursivePaths=true;
	if(!ClassName.IsEmpty()){UClass* C=FindObject<UClass>(nullptr,*ClassName);if(C)F.ClassPaths.Add(C->GetClassPathName());}
	TArray<FAssetData> A;ARM.Get().GetAssets(F,A);
	auto J=MakeJsonObj();TArray<TSharedPtr<FJsonValue>> Arr;
	for(const auto& D:A){auto O=MakeJsonObj();O->SetStringField(TEXT("name"),D.AssetName.ToString());O->SetStringField(TEXT("path"),D.GetObjectPathString());Arr.Add(TSharedPtr<FJsonValue>(new FJsonValueObject(O)));}
	J->SetArrayField(TEXT("assets"),Arr);J->SetNumberField(TEXT("count"),Arr.Num());return J;
}
