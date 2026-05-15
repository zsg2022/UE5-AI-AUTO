// Advanced tools: MetaSound, Niagara, Sequencer, PCG, Landscape, Asset management
#pragma once
#include "CoreMinimal.h"

struct FUE5AIAUTOAdvancedTools
{
	// === MetaSound ===
	static TSharedPtr<FJsonObject> CreateMetaSoundSource(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddMetaSoundNode(const FString& Path, const FString& NodeNamespace, const FString& NodeName);
	static TSharedPtr<FJsonObject> ConnectMetaSoundNodes(const FString& Path, const FString& FromNode, const FString& FromOutput, const FString& ToNode, const FString& ToInput);
	static TSharedPtr<FJsonObject> BuildMetaSound(const FString& Path);

	// === Niagara ===
	static TSharedPtr<FJsonObject> CreateNiagaraSystem(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddNiagaraEmitter(const FString& Path, const FString& EmitterTemplatePath);
	static TSharedPtr<FJsonObject> SetNiagaraParameter(const FString& Path, const FString& ParamName, float Value);

	// === Sequencer ===
	static TSharedPtr<FJsonObject> CreateLevelSequence(const FString& Path, const FString& Name);
	static TSharedPtr<FJsonObject> AddActorToSequencer(const FString& SeqPath, const FString& ActorName);
	static TSharedPtr<FJsonObject> AddTransformTrack(const FString& SeqPath, const FString& ActorName);
	static TSharedPtr<FJsonObject> SetKeyframe(const FString& SeqPath, const FString& ActorName, double Time, double X, double Y, double Z);

	// === PCG (disabled - plugin not available in project) ===
	// static TSharedPtr<FJsonObject> CreatePCGGraph(const FString& Path, const FString& Name);
	// static TSharedPtr<FJsonObject> AddPCGNode(const FString& Path, const FString& NodeClass);

	// === Landscape ===
	static TSharedPtr<FJsonObject> CreateLandscape(const FString& LevelName, int32 Size, int32 Sections);
	static TSharedPtr<FJsonObject> ImportHeightmap(const FString& LandscapeActor, const FString& HeightmapPath);

	// === Asset Management ===
	static TSharedPtr<FJsonObject> CopyAsset(const FString& SourcePath, const FString& DestPath);
	static TSharedPtr<FJsonObject> DeleteAsset(const FString& Path);
	static TSharedPtr<FJsonObject> ListAssets(const FString& BasePath, const FString& ClassName);
};