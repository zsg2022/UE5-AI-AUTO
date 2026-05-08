// Copyright GU5. All Rights Reserved.

#include "UE5AIAUTOEditorSubsystem.h"
#include "UE5AIAUTOCommandExecutor.h"
#include "UE5AIAUTOReflectionScanner.h"
#include "UE5AIAUTOWebSocketClient.h"
#include "UE5AIAUTOMaterialEditor.h"
#include "UE5AIAUTOBlueprintEditor.h"
#include "UE5AIAUTOCppTools.h"
#include "UE5AIAUTOAdvancedTools.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"

// Lifecycle

void UUE5AIAUTOEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] EditorSubsystem Initialized"));

	CommandExecutor = MakeUnique<FUE5AIAUTOCommandExecutor>();
	RegisterCommandHandlers();

	WebSocketClient = MakeUnique<FUE5AIAUTOWebSocketClient>(TEXT("127.0.0.1"), 9876);
	WebSocketClient->OnConnected.AddLambda([this]()
	{
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Connected to MCP Server"));
	});
	WebSocketClient->OnDisconnected.AddLambda([](const FString& Reason)
	{
		UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Disconnected: %s"), *Reason);
	});
	WebSocketClient->OnResponse.AddLambda([this](const FGuid& Id, const FString& Json)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(R, Obj) || !Obj.IsValid()) return;

		if (!Obj->HasField(TEXT("method"))) return;
			FString Method = Obj->GetStringField(TEXT("method"));
		TSharedPtr<FJsonObject> Params = Obj->GetObjectField(TEXT("params"));
		if (!Params.IsValid()) Params = MakeShareable(new FJsonObject());

		if (!GEditor) { TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject()); Err->SetStringField(TEXT("error"), TEXT("Editor not available")); return; }

		TSharedPtr<FJsonObject> Result = CommandExecutor->ExecuteImmediate(Method, Params);

			FString ResultJson;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ResultJson);
			FJsonSerializer::Serialize(Result.ToSharedRef(), W);
			FString Resp = TEXT("{\"id\":\"") + Id.ToString(EGuidFormats::DigitsWithHyphens) + TEXT("\",\"result\":") + ResultJson + TEXT("}");
		WebSocketClient->SendCommand(Id, Resp);
	});
	WebSocketClient->Connect();

	TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UUE5AIAUTOEditorSubsystem::Tick));
}

void UUE5AIAUTOEditorSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] EditorSubsystem Deinitialized"));
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	}
	if (WebSocketClient)
	{
		WebSocketClient->Disconnect();
		WebSocketClient.Reset();
	}
	CommandExecutor.Reset();
	Super::Deinitialize();
}

bool UUE5AIAUTOEditorSubsystem::Tick(float DeltaTime)
{
	if (CommandExecutor)
	{
		CommandExecutor->Tick(DeltaTime);
	}
	return true;
}

// Command Handlers

void UUE5AIAUTOEditorSubsystem::RegisterCommandHandlers()
{
	// --- ping ---
	CommandExecutor->RegisterHandler(TEXT("ping"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("status"), TEXT("ok"));
			return Result;
		});

	// --- scan_reflection ---
	CommandExecutor->RegisterHandler(TEXT("scan_reflection"),
		[this](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FUE5AIAUTOReflectionScanner::FScanResult ScanResult =
				FUE5AIAUTOReflectionScanner::ScanAllModules();
			const FString CachePath = FPaths::ProjectSavedDir() /
				TEXT("UE5AIAUTO") / TEXT("reflection_cache.json");
			FUE5AIAUTOReflectionScanner::SaveCacheToJson(ScanResult, CachePath);

			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetNumberField(TEXT("function_count"), ScanResult.FunctionCount);
			Result->SetNumberField(TEXT("class_count"), ScanResult.ClassNames.Num());
			Result->SetStringField(TEXT("cache_path"), CachePath);
			return Result;
		});

	// --- create_actor ---
	CommandExecutor->RegisterHandler(TEXT("create_actor"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			const FString ClassName = Params->GetStringField(TEXT("class_name"));
			const TSharedPtr<FJsonObject>* LocObj = nullptr;
			FVector Location = FVector::ZeroVector;
			if (Params->TryGetObjectField(TEXT("location"), LocObj))
			{
				Location.X = (*LocObj)->GetNumberField(TEXT("x"));
				Location.Y = (*LocObj)->GetNumberField(TEXT("y"));
				Location.Z = (*LocObj)->GetNumberField(TEXT("z"));
			}

			UClass* Class = UClass::TryFindTypeSlow<UClass>(ClassName);
			if (!Class)
			{
				Class = UClass::TryFindTypeSlow<UClass>(FString(TEXT("A")) + ClassName);
			}
			if (!Class)
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Class not found: %s"), *ClassName));
				Err->SetNumberField(TEXT("error_code"), static_cast<int32>(EUE5AIAUTOErrorCode::NOT_FOUND));
				return Err;
			}

			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World)
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), TEXT("World not available"));
				Err->SetNumberField(TEXT("error_code"), static_cast<int32>(EUE5AIAUTOErrorCode::INTERNAL));
				return Err;
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* Actor = World->SpawnActor<AActor>(Class, Location, FRotator::ZeroRotator, SpawnParams);
			if (!Actor)
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), FString::Printf(TEXT("SpawnActor failed: %s"), *ClassName));
				return Err;
			}

			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("actor_name"), Actor->GetName());
			Result->SetStringField(TEXT("class_name"), Actor->GetClass()->GetName());
			TSharedPtr<FJsonObject> LocResult = MakeShareable(new FJsonObject());
			LocResult->SetNumberField(TEXT("x"), Actor->GetActorLocation().X);
			LocResult->SetNumberField(TEXT("y"), Actor->GetActorLocation().Y);
			LocResult->SetNumberField(TEXT("z"), Actor->GetActorLocation().Z);
			Result->SetObjectField(TEXT("location"), LocResult);
			return Result;
		});

	// --- modify_actor_property ---
	CommandExecutor->RegisterHandler(TEXT("modify_actor_property"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			const FString ActorName = Params->GetStringField(TEXT("actor_name"));
			const FString PropName = Params->GetStringField(TEXT("property_name"));
			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World)
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), TEXT("World not available"));
				return Err;
			}

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Actor = *It;
				if (Actor->GetName() != ActorName) continue;

				FProperty* Prop = Actor->GetClass()->FindPropertyByName(*PropName);
				if (!Prop)
				{
					TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
					Err->SetStringField(TEXT("error"),
						FString::Printf(TEXT("Property not found: %s::%s"), *ActorName, *PropName));
					return Err;
				}

				TSharedPtr<FJsonValue> ValPtr = Params->TryGetField(TEXT("value"));
				if (ValPtr.IsValid())
				{
					if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
						BP->SetPropertyValue_InContainer(Actor, ValPtr->AsBool());
					else if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
						FP->SetPropertyValue_InContainer(Actor, ValPtr->AsNumber());
					else if (FIntProperty* IP = CastField<FIntProperty>(Prop))
						IP->SetPropertyValue_InContainer(Actor, (int32)ValPtr->AsNumber());
					else if (FStrProperty* SP = CastField<FStrProperty>(Prop))
						SP->SetPropertyValue_InContainer(Actor, ValPtr->AsString());
				}

				TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
				Result->SetStringField(TEXT("actor_name"), ActorName);
				Result->SetStringField(TEXT("property"), PropName);
				Result->SetStringField(TEXT("status"), TEXT("modified"));
				return Result;
			}

			TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
			Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Actor not found: %s"), *ActorName));
			return Err;
		});

	// --- delete_actor ---
	CommandExecutor->RegisterHandler(TEXT("delete_actor"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			const FString ActorName = Params->GetStringField(TEXT("actor_name"));
			UWorld* World = GEditor->GetEditorWorldContext().World();
			if (!World)
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), TEXT("World not available"));
				return Err;
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if ((*It)->GetName() == ActorName)
				{
					World->DestroyActor(*It);
					TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
					Result->SetStringField(TEXT("actor_name"), ActorName);
					Result->SetStringField(TEXT("status"), TEXT("deleted"));
					return Result;
				}
			}
			TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
			Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Actor not found: %s"), *ActorName));
			return Err;
		});

	// --- query_scene ---
	CommandExecutor->RegisterHandler(TEXT("query_scene"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			UWorld* World = GEditor->GetEditorWorldContext().World();
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			TArray<TSharedPtr<FJsonValue>> ActorArray;
			if (World)
			{
				for (TActorIterator<AActor> It(World); It; ++It)
				{
					AActor* Actor = *It;
					TSharedPtr<FJsonObject> ActorObj = MakeShareable(new FJsonObject());
					ActorObj->SetStringField(TEXT("name"), Actor->GetName());
					ActorObj->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
					TSharedPtr<FJsonObject> LocObj = MakeShareable(new FJsonObject());
					LocObj->SetNumberField(TEXT("x"), Actor->GetActorLocation().X);
					LocObj->SetNumberField(TEXT("y"), Actor->GetActorLocation().Y);
					LocObj->SetNumberField(TEXT("z"), Actor->GetActorLocation().Z);
					ActorObj->SetObjectField(TEXT("location"), LocObj);
					ActorArray.Add(MakeShareable(new FJsonValueObject(ActorObj)));
				}
			}
			Result->SetArrayField(TEXT("actors"), ActorArray);
			Result->SetNumberField(TEXT("count"), ActorArray.Num());
			return Result;
		});

	// --- set_viewport_camera ---
	CommandExecutor->RegisterHandler(TEXT("set_viewport_camera"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			const TSharedPtr<FJsonObject>* LocObj = nullptr;
			if (!Params->TryGetObjectField(TEXT("location"), LocObj))
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), TEXT("Missing location"));
				return Err;
			}
			FVector Loc((*LocObj)->GetNumberField(TEXT("x")),
				(*LocObj)->GetNumberField(TEXT("y")),
				(*LocObj)->GetNumberField(TEXT("z")));

			FViewport* VP = GEditor->GetActiveViewport();
			if (VP)
			{
				FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(VP->GetClient());
				if (Client)
				{
					Client->SetViewLocation(Loc);
				}
			}
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("status"), TEXT("ok"));
			return Result;
		});

	// === BLUEPRINT HANDLERS ===
#define BP_REG(NAME, Fn) CommandExecutor->RegisterHandler(TEXT(NAME), \
	[](TSharedPtr<FJsonObject> P)->TSharedPtr<FJsonObject>{ return Fn; })

	BP_REG("bp_list", FUE5AIAUTOBlueprintEditor::ListBlueprints(P->GetStringField(TEXT("path"))));
	BP_REG("bp_open", FUE5AIAUTOBlueprintEditor::OpenBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create", FUE5AIAUTOBlueprintEditor::CreateBlueprint(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("parent_class"))));
	BP_REG("bp_compile", FUE5AIAUTOBlueprintEditor::CompileBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_list_nodes", FUE5AIAUTOBlueprintEditor::ListNodes(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph"))));
	BP_REG("bp_create_node",
		FUE5AIAUTOBlueprintEditor::CreateNode(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")),
			P->GetStringField(TEXT("node_class")), P->GetNumberField(TEXT("x")), P->GetNumberField(TEXT("y")),
			P->GetObjectField(TEXT("defaults"))));
	BP_REG("bp_remove_node", FUE5AIAUTOBlueprintEditor::RemoveNode(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id"))));
	BP_REG("bp_set_pin_default", FUE5AIAUTOBlueprintEditor::SetPinDefault(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id")), P->GetStringField(TEXT("pin")), P->GetStringField(TEXT("value"))));
	BP_REG("bp_connect_pins", FUE5AIAUTOBlueprintEditor::ConnectPins(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("src_node")), P->GetStringField(TEXT("src_pin")), P->GetStringField(TEXT("dst_node")), P->GetStringField(TEXT("dst_pin"))));
	BP_REG("bp_disconnect_pin", FUE5AIAUTOBlueprintEditor::DisconnectPin(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("graph")), P->GetStringField(TEXT("node_id")), P->GetStringField(TEXT("pin"))));
	BP_REG("bp_list_variables", FUE5AIAUTOBlueprintEditor::ListVariables(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_variable", FUE5AIAUTOBlueprintEditor::CreateVariable(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetStringField(TEXT("type")), P->GetBoolField(TEXT("is_array"))));
	BP_REG("bp_remove_variable", FUE5AIAUTOBlueprintEditor::RemoveVariable(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_set_variable_default", FUE5AIAUTOBlueprintEditor::SetVariableDefault(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetStringField(TEXT("value"))));
	BP_REG("bp_list_functions", FUE5AIAUTOBlueprintEditor::ListFunctions(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_function", FUE5AIAUTOBlueprintEditor::CreateFunction(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name")), P->GetArrayField(TEXT("inputs")), P->GetArrayField(TEXT("outputs"))));
	BP_REG("bp_remove_function", FUE5AIAUTOBlueprintEditor::RemoveFunction(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_list_graphs", FUE5AIAUTOBlueprintEditor::ListGraphs(P->GetStringField(TEXT("path"))));
	BP_REG("bp_list_node_classes", FUE5AIAUTOBlueprintEditor::ListNodeClasses());
	BP_REG("bp_list_components", FUE5AIAUTOBlueprintEditor::ListComponents(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_component", FUE5AIAUTOBlueprintEditor::AddComponent(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("component_class")), P->GetStringField(TEXT("variable_name"))));
	BP_REG("bp_remove_component", FUE5AIAUTOBlueprintEditor::RemoveComponent(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_list_interfaces", FUE5AIAUTOBlueprintEditor::ListInterfaces(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_interface", FUE5AIAUTOBlueprintEditor::AddInterface(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("interface_class"))));
	BP_REG("bp_remove_interface", FUE5AIAUTOBlueprintEditor::RemoveInterface(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("interface_class"))));
	BP_REG("bp_list_macros", FUE5AIAUTOBlueprintEditor::ListMacros(P->GetStringField(TEXT("path"))));
	BP_REG("bp_add_event_dispatcher", FUE5AIAUTOBlueprintEditor::AddEventDispatcher(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_save", FUE5AIAUTOBlueprintEditor::SaveBlueprint(P->GetStringField(TEXT("path"))));
	BP_REG("bp_create_macro", FUE5AIAUTOBlueprintEditor::CreateMacro(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_remove_macro", FUE5AIAUTOBlueprintEditor::RemoveMacro(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	BP_REG("bp_remove_event_dispatcher", FUE5AIAUTOBlueprintEditor::RemoveEventDispatcher(P->GetStringField(TEXT("path")), P->GetStringField(TEXT("name"))));
	CommandExecutor->RegisterHandler(TEXT("bt_create"), [](auto P){return FUE5AIAUTOCppTools::CreateBehaviorTree(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("create_blackboard"), [](auto P){return FUE5AIAUTOCppTools::CreateBlackboard(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_bb_key"), [](auto P){return FUE5AIAUTOCppTools::AddBlackboardKey(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("key_name")),P->GetStringField(TEXT("key_type")));});
	CommandExecutor->RegisterHandler(TEXT("create_widget"), [](auto P){return FUE5AIAUTOCppTools::CreateWidgetBlueprint(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_widget_to_canvas"), [](auto P){return FUE5AIAUTOCppTools::AddWidgetToCanvas(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("widget_class")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_action_mapping"), [](auto P){return FUE5AIAUTOCppTools::AddActionMapping(P->GetStringField(TEXT("action_name")),P->GetStringField(TEXT("key")));});
	CommandExecutor->RegisterHandler(TEXT("add_axis_mapping"), [](auto P){return FUE5AIAUTOCppTools::AddAxisMapping(P->GetStringField(TEXT("axis_name")),P->GetStringField(TEXT("key")),P->GetNumberField(TEXT("scale")));});
	CommandExecutor->RegisterHandler(TEXT("enable_physics"), [](auto P){return FUE5AIAUTOCppTools::EnablePhysics(P->GetStringField(TEXT("actor_name")),P->GetBoolField(TEXT("enable")));});
	CommandExecutor->RegisterHandler(TEXT("set_mass"), [](auto P){return FUE5AIAUTOCppTools::SetMass(P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("mass")));});
	CommandExecutor->RegisterHandler(TEXT("play_sound"), [](auto P){return FUE5AIAUTOCppTools::PlaySound(P->GetStringField(TEXT("actor_name")),P->GetStringField(TEXT("sound_path")));});
	CommandExecutor->RegisterHandler(TEXT("find_path"), [](auto P){return FUE5AIAUTOCppTools::FindPathTo(P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("z")));});
	CommandExecutor->RegisterHandler(TEXT("create_metasound"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateMetaSoundSource(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("create_niagara"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateNiagaraSystem(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_emitter"), [](auto P){return FUE5AIAUTOAdvancedTools::AddNiagaraEmitter(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("emitter_path")));});
	CommandExecutor->RegisterHandler(TEXT("set_niagara_param"), [](auto P){return FUE5AIAUTOAdvancedTools::SetNiagaraParameter(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("param_name")),P->GetNumberField(TEXT("value")));});
	CommandExecutor->RegisterHandler(TEXT("create_sequence"), [](auto P){return FUE5AIAUTOAdvancedTools::CreateLevelSequence(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("add_transform_track"), [](auto P){return FUE5AIAUTOAdvancedTools::AddTransformTrack(P->GetStringField(TEXT("seq_path")),P->GetStringField(TEXT("actor_name")));});
	CommandExecutor->RegisterHandler(TEXT("set_keyframe"), [](auto P){return FUE5AIAUTOAdvancedTools::SetKeyframe(P->GetStringField(TEXT("seq_path")),P->GetStringField(TEXT("actor_name")),P->GetNumberField(TEXT("time")),P->GetNumberField(TEXT("x")),P->GetNumberField(TEXT("y")),P->GetNumberField(TEXT("z")));});
	CommandExecutor->RegisterHandler(TEXT("create_pcg"), [](auto P){return FUE5AIAUTOAdvancedTools::CreatePCGGraph(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("name")));});
	CommandExecutor->RegisterHandler(TEXT("copy_asset"), [](auto P){return FUE5AIAUTOAdvancedTools::CopyAsset(P->GetStringField(TEXT("source_path")),P->GetStringField(TEXT("dest_path")));});
	CommandExecutor->RegisterHandler(TEXT("delete_asset"), [](auto P){return FUE5AIAUTOAdvancedTools::DeleteAsset(P->GetStringField(TEXT("path")));});
	CommandExecutor->RegisterHandler(TEXT("list_assets"), [](auto P){return FUE5AIAUTOAdvancedTools::ListAssets(P->GetStringField(TEXT("path")),P->GetStringField(TEXT("class_name")));});
#undef BP_REG

	// --- create_material ---
	CommandExecutor->RegisterHandler(TEXT("create_material"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			FMaterialCreateParams MatParams;
			MatParams.AssetPath = Params->GetStringField(TEXT("asset_path"));
			Params->TryGetStringField(TEXT("base_color_hex"), MatParams.BaseColorHex);
			Params->TryGetNumberField(TEXT("opacity"), MatParams.Opacity);
			Params->TryGetNumberField(TEXT("roughness"), MatParams.Roughness);
			Params->TryGetNumberField(TEXT("metallic"), MatParams.Metallic);
			Params->TryGetNumberField(TEXT("specular"), MatParams.Specular);

			if (MatParams.AssetPath.IsEmpty())
			{
				TSharedPtr<FJsonObject> Err = MakeShareable(new FJsonObject());
				Err->SetStringField(TEXT("error"), TEXT("asset_path is required"));
				return Err;
			}
			return FUE5AIAUTOMaterialEditor::CreatePbrMaterial(MatParams);
		});

	// --- screenshot ---
	CommandExecutor->RegisterHandler(TEXT("screenshot"),
		[](TSharedPtr<FJsonObject> Params) -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject());
			Result->SetStringField(TEXT("status"), TEXT("screenshot_requested"));
			Result->SetStringField(TEXT("note"), TEXT("Binary frame transmission"));
			return Result;
		});

	UE_LOG(LogTemp, Log, TEXT("[UE5AIAUTO] Handlers registered: %d"),
		CommandExecutor->GetRegisteredMethods().Num());
}
