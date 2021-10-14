// Fill out your copyright notice in the Description page of Project Settings.


#include "VRCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"

// Sets default values
AVRCharacter::AVRCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Set up Scene component as root
	VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
	VRRoot->SetupAttachment(GetRootComponent());

	//Attach camera to VRRoot
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VRRoot);

	DestinationMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestinationMarker"));
	DestinationMarker->SetupAttachment(VRRoot);
}

// Called when the game starts or when spawned
void AVRCharacter::BeginPlay()
{
	Super::BeginPlay();

	DestinationMarker->SetVisibility(false);
	
}

// Called every frame
void AVRCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Calculate Camera offset every frame but getting Camera Component location and subtract ActorLocation which returns location of RootComponent of Actor (in this case the capsule)
	FVector NewCameraOffset = Camera->GetComponentLocation() - GetActorLocation();
	NewCameraOffset.Z = 0; //do not offset vertical axis
	AddActorWorldOffset(NewCameraOffset); //Offset the whole actor by amount NewCameraOffset
	VRRoot->AddWorldOffset(-NewCameraOffset); //Negate the actor offset for the VRRoot offset to keep this one component in the same place

	UpdateDestinationMarker();
}

void AVRCharacter::UpdateDestinationMarker()
{
	FVector Location;
	bool bHasDestination = FindTeleportDestination(Location);

	if (bHasDestination) //turn on destination marker if line tracing is colliding and is on the navmesh plane
	{
		DestinationMarker->SetVisibility(true);

		DestinationMarker->SetWorldLocation(Location);
	}
	else
	{
		DestinationMarker->SetVisibility(false);
	}
}

bool AVRCharacter::FindTeleportDestination(FVector &OutLocation)
{
	//Declare and initialize FVector variables for arguments to LineTraceSingleByChannel below
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * MaxTeleportDistance;

	FHitResult HitResult;
	//UseLinetraceSingleByChannel to return just the first collision in the trace, stored in variable HitResult
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility);

	if (!bHit) return false; //cancel rest of function if there is no hit

	FNavLocation NavLocation;
	bool bOnNavMesh = UNavigationSystemV1::GetNavigationSystem(GetWorld())->ProjectPointToNavigation(HitResult.Location, NavLocation, TeleportProjectionExtent);
	//https://docs.unrealengine.com/4.26/en-US/API/Runtime/NavigationSystem/UNavigationSystemV1/
	//https://docs.unrealengine.com/4.26/en-US/API/Runtime/NavigationSystem/UNavigationSystemV1/ProjectPointToNavigation/2/

	if (!bOnNavMesh) return false;

	OutLocation = NavLocation.Location;

	return true; //return true if both bools in the function are true
}


// Called to bind functionality to input
void AVRCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("Forward"), this, &AVRCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("Right"), this, &AVRCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Teleport"), IE_Released, this, &AVRCharacter::BeginTeleport);
	//https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Components/UInputComponent/BindAction/
}

void AVRCharacter::MoveForward(float throttle)
{
	AddMovementInput(throttle * Camera->GetForwardVector());
}

void AVRCharacter::MoveRight(float throttle)
{
	AddMovementInput(throttle * Camera->GetRightVector());
}

void AVRCharacter::BeginTeleport()
{
	//Get ref to player controller by casting to player controller and calling GetController method
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr) //is valid check
	{
		//start Camera fade
		//https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/Camera/APlayerCameraManager/StartCameraFade/
		PC->PlayerCameraManager->StartCameraFade(0, 1, TeleportFadeTime, FLinearColor::Black);
	}

	//Set a timer to call FinishTeleport
	FTimerHandle Handle;
	//https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/GameFramework/AActor/GetWorldTimerManager/
	//https://docs.unrealengine.com/4.27/en-US/API/Runtime/Engine/FTimerManager/SetTimer/5/
	GetWorldTimerManager().SetTimer(Handle, this, &AVRCharacter::FinishTeleport, TeleportFadeTime);
	
}

void AVRCharacter::FinishTeleport()
{
	float ScaledCapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocation(DestinationMarker->GetComponentLocation() + (0, 0, ScaledCapsuleHalfHeight)); //Adding two FVectors, second is for height
	UE_LOG(LogTemp, Warning, TEXT("Teleported with scaled capsule height: %f"), ScaledCapsuleHalfHeight);

	//finish fade
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC != nullptr)
	{
		PC->PlayerCameraManager->StartCameraFade(1, 0, TeleportFadeTime, FLinearColor::Black);
	}
}