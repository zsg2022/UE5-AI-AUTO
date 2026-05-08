// C++ native tools - Input, Physics, Audio, Navigation
#include "UE5AIAUTOCppTools.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/InputSettings.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

static TSharedPtr<FJsonObject> R(){return TSharedPtr<FJsonObject>(new FJsonObject());}
static TSharedPtr<FJsonObject> Err(const FString& M){auto J=TSharedPtr<FJsonObject>(new FJsonObject());J->SetStringField(TEXT("error"),M);return J;}

// === Input ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddActionMapping(const FString& N, const FString& K){
	auto J=R();UInputSettings::GetInputSettings()->AddActionMapping(FInputActionKeyMapping(FName(*N),FKey(*K)),false);UInputSettings::GetInputSettings()->SaveKeyMappings();J->SetStringField(TEXT("status"),TEXT("added"));return J;
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAxisMapping(const FString& N, const FString& K, float S){
	auto J=R();UInputSettings::GetInputSettings()->AddAxisMapping(FInputAxisKeyMapping(FName(*N),FKey(*K),S),false);UInputSettings::GetInputSettings()->SaveKeyMappings();J->SetStringField(TEXT("status"),TEXT("added"));return J;
}

// === Physics ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::EnablePhysics(const FString& An, bool bE){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->SetSimulatePhysics(bE);auto J=R();J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return Err("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::SetMass(const FString& An, float M){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->SetMassOverrideInKg(NAME_None,M,true);auto J=R();J->SetNumberField(TEXT("mass"),M);J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return Err("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::ApplyForce(const FString& An, double X, double Y, double Z){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UPrimitiveComponent*> Cs;(*It)->GetComponents<UPrimitiveComponent>(Cs);for(auto* C:Cs)C->AddForce(FVector(X,Y,Z),NAME_None,true);auto J=R();J->SetStringField(TEXT("status"),TEXT("ok"));return J;}}
	return Err("Not found");
}

// === Audio ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::PlaySound(const FString& An, const FString& SP){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	auto* S=LoadObject<USoundBase>(nullptr,*SP);if(!S)return Err("Sound not found");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){UGameplayStatics::SpawnSoundAttached(S,(*It)->GetRootComponent());auto J=R();J->SetStringField(TEXT("status"),TEXT("playing"));return J;}}
	return Err("Not found");
}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::StopSound(const FString& An){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()==An){TArray<UAudioComponent*> As;(*It)->GetComponents<UAudioComponent>(As);for(auto* A:As)A->Stop();auto J=R();J->SetStringField(TEXT("status"),TEXT("stopped"));return J;}}
	return Err("Not found");
}

// === Navigation ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::FindPathTo(const FString& An, double X, double Y, double Z){
	UWorld* W=GEditor?GEditor->GetEditorWorldContext().World():nullptr;if(!W)return Err("No world");
	UNavigationSystemV1* NS=FNavigationSystem::GetCurrent<UNavigationSystemV1>(W);if(!NS)return Err("No NavSystem");
	for(TActorIterator<AActor> It(W);It;++It){if((*It)->GetName()!=An)continue;
		FPathFindingQuery Q;Q.StartLocation=(*It)->GetActorLocation();Q.EndLocation=FVector(X,Y,Z);Q.NavData=NS->GetDefaultNavDataInstance();
		FPathFindingResult Res=NS->FindPathSync(Q);
		auto J=R();
		if(Res.IsSuccessful()){TArray<TSharedPtr<FJsonValue>> Pts;for(auto& Pt:Res.Path->GetPathPoints()){auto PJ=TSharedPtr<FJsonObject>(new FJsonObject());PJ->SetNumberField(TEXT("x"),Pt.Location.X);PJ->SetNumberField(TEXT("y"),Pt.Location.Y);PJ->SetNumberField(TEXT("z"),Pt.Location.Z);Pts.Add(MakeShareable(new FJsonValueObject(PJ)));}J->SetArrayField(TEXT("path"),Pts);J->SetNumberField(TEXT("points"),Pts.Num());}
		else{J->SetStringField(TEXT("error"),TEXT("No path found"));}
		return J;
	}
	return Err("Not found");
}

// === Stubs (editor-only UIs) ===
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateBehaviorTree(const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BT editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateBlackboard(const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BB editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBlackboardKey(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BB editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::CreateWidgetBlueprint(const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open UMG editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddWidgetToCanvas(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open Widget Designer"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTComposite(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BT editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTTask(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BT editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTDecorator(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BT editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddBTService(const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open BT editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAnimState(const FString&,const FString&,double,double){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open AnimBP editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::AddAnimTransition(const FString&,const FString&,const FString&,const FString&){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open AnimBP editor"));return J;}
TSharedPtr<FJsonObject> FUE5AIAUTOCppTools::SetAnimGraphNode(const FString&,const FString&,double,double){auto J=R();J->SetStringField(TEXT("info"),TEXT("Open AnimBP editor"));return J;}
