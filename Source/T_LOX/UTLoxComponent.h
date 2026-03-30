// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "tlox_gameplay.h"
#include "UTLoxComponent.generated.h"



DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGoalSatisfied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoveCompleted, bool, bMoved);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMoveFailed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUndoPerformed);

USTRUCT(BlueprintType)
struct FCellInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) int32 X;
	UPROPERTY(BlueprintReadOnly) int32 Y;
	UPROPERTY(BlueprintReadOnly) int32 Z;
	UPROPERTY(BlueprintReadOnly) uint8 Type; // 0=Empty, 1=Solid, 2=Goal
};

USTRUCT(BlueprintType)
struct FPieceCellTransform
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FVector Position;
	UPROPERTY(BlueprintReadOnly) FRotator Rotation;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class T_LOX_API UUTLoxComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UUTLoxComponent();
	virtual ~UUTLoxComponent();

	UPROPERTY(BlueprintAssignable, Category = "T-Lox")
	FOnGoalSatisfied OnGoalSatisfied;

	UPROPERTY(BlueprintAssignable, Category = "T-Lox")
	FOnMoveCompleted OnMoveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "T-Lox")
	FOnMoveFailed OnMoveFailed;

	UPROPERTY(BlueprintAssignable, Category = "T-Lox")
	FOnUndoPerformed OnUndoPerformed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "T-Lox")
	float GridSpacingCm = 100.f;


	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void LoadAndInitLevel(const FString& LevelPath);

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void MoveLeft();

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void MoveRight();

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void MoveForward();

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void MoveBackward();

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void ResetLevel();

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void UndoMove();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "T-Lox")
	bool CanUndo() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	bool IsAnimating() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	bool IsGoalMet() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	int32 GetMoveCount() const;

	const PieceAnimationState& GetAnimState() const;
	const GameplayState& GetGameState() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	TArray<FCellInfo> GetAllCells() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	FVector CellToWorld(int32 X, int32 Y, int32 Z) const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void GetPieceCellPositions(TArray<FVector>& OutPositions) const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void GetAnimatedPieceTransforms(TArray<FPieceCellTransform>& OutTransforms) const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	FVector GetLevelCenterWorld() const;

	UFUNCTION(BlueprintCallable, Category = "T-Lox")
	void RotateCameraAroundCenter(AActor* CameraActor, float AngleDegrees) const;

protected:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	GameplayState       State;
	GameplayState       InitialState;
	PieceAnimationState AnimState;
	std::vector<GameplayState> StateHistory;

	void RequestMove(MoveDir Dir);
		
};
