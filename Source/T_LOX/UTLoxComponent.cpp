#include "UTLoxComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

UUTLoxComponent::UUTLoxComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

UUTLoxComponent::~UUTLoxComponent() = default;

void UUTLoxComponent::LoadAndInitLevel(const FString& LevelPath)
{
    Level L = LoadLevel(TCHAR_TO_UTF8(*LevelPath));
    State = CreateGameplayState(L);
    InitialState = State;
    AnimState = {};
    StateHistory.clear();
}

void UUTLoxComponent::RequestMove(MoveDir IntendedDir)
{
    if (AnimState.active) return;

    MoveDir WorldDir = IntendedDir;
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        if (APlayerCameraManager* CamManager = PC->PlayerCameraManager)
        {
            FVector CamFwd = CamManager->GetCameraRotation().Vector();
            CamFwd.Z = 0.0f; CamFwd.Normalize();
            FVector CamRt = FRotationMatrix(CamManager->GetCameraRotation()).GetScaledAxis(EAxis::Y);
            CamRt.Z = 0.0f; CamRt.Normalize();

            // SKEW the input by 45 degrees to perfectly align WASD to the isometric grid gaps
            // This prevents diagonal camera views from causing ambiguous edge-flipping.
            CamFwd = CamFwd.RotateAngleAxis(25.f, FVector::UpVector);
            CamRt = CamRt.RotateAngleAxis(25.f, FVector::UpVector);

            FVector InputVector = FVector::ZeroVector;
            if (IntendedDir == MoveDir::Forward) InputVector = -CamFwd;
            if (IntendedDir == MoveDir::Backward) InputVector = CamFwd;
            if (IntendedDir == MoveDir::Right) InputVector = CamRt;
            if (IntendedDir == MoveDir::Left) InputVector = -CamRt;

            if (!InputVector.IsNearlyZero())
            {
                float MaxDot = InputVector.X; WorldDir = MoveDir::Right;
                if (-InputVector.X > MaxDot) { MaxDot = -InputVector.X; WorldDir = MoveDir::Left; }
                if (InputVector.Y > MaxDot) { MaxDot = InputVector.Y; WorldDir = MoveDir::Forward; }
                if (-InputVector.Y > MaxDot) { MaxDot = -InputVector.Y; WorldDir = MoveDir::Backward; }
            }
        }
    }

    GameplayState PrevState = State;
    MoveResult Result = TryMove(State, WorldDir);
    if (Result.moved)
    {
        StateHistory.push_back(PrevState);
    }
    BeginMoveAnimation(AnimState, Result, WorldDir);
    OnMoveCompleted.Broadcast(Result.moved);
    if (!Result.moved)
        OnMoveFailed.Broadcast();
    if (IsGoalSatisfied(State))
        OnGoalSatisfied.Broadcast();
}

void UUTLoxComponent::MoveLeft() { RequestMove(MoveDir::Left); }
void UUTLoxComponent::MoveRight() { RequestMove(MoveDir::Right); }
void UUTLoxComponent::MoveForward() { RequestMove(MoveDir::Forward); }
void UUTLoxComponent::MoveBackward() { RequestMove(MoveDir::Backward); }

void UUTLoxComponent::ResetLevel()
{
    State = InitialState;
    AnimState = {};
    StateHistory.clear();
}

bool UUTLoxComponent::IsAnimating() const { return AnimState.active; }
bool UUTLoxComponent::IsGoalMet()   const { return IsGoalSatisfied(State); }
int32 UUTLoxComponent::GetMoveCount() const { return State.moveCount; }

void UUTLoxComponent::UndoMove()
{
    if (CanUndo())
    {
        State = StateHistory.back();
        StateHistory.pop_back();
        AnimState = {};
        OnUndoPerformed.Broadcast();
    }
}

bool UUTLoxComponent::CanUndo() const
{
    return !AnimState.active && !StateHistory.empty();
}

void UUTLoxComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateAnimation(AnimState, DeltaTime);

    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        float RotateSpeed = 90.0f; // degrees per second
        float YawDelta = 0.0f;

        if (PC->IsInputKeyDown(EKeys::Q))
        {
            YawDelta += RotateSpeed * DeltaTime;
        }
        if (PC->IsInputKeyDown(EKeys::E))
        {
            YawDelta -= RotateSpeed * DeltaTime;
        }

        if (PC->WasInputKeyJustPressed(EKeys::U))
        {
            UndoMove();
        }

        if (FMath::Abs(YawDelta) > 0.001f)
        {
            AActor* Target = PC->GetPawn() ? PC->GetPawn() : PC->GetViewTarget();
            if (Target)
            {
                RotateCameraAroundCenter(Target, YawDelta);
            }
        }
    }

    if (!AnimState.active && State.level.sizeX > 0)
    {
        MoveDir Dirs[] = { MoveDir::Left, MoveDir::Right, MoveDir::Forward, MoveDir::Backward };
        float HalfGrid = GridSpacingCm * 0.5f;

        for (MoveDir Dir : Dirs)
        {
            GameplayState TempState = State; 
            MoveResult Result = TryMove(TempState, Dir);

            FVector PivotWorld = CellToWorld(Result.pivot.x, Result.pivot.y, Result.pivot.z);
            FVector PivotOffset = FVector::ZeroVector;

            switch (Dir)
            {
            case MoveDir::Left:     PivotOffset = FVector(-HalfGrid, 0, -HalfGrid); break;
            case MoveDir::Right:    PivotOffset = FVector(HalfGrid, 0, -HalfGrid); break;
            case MoveDir::Forward:  PivotOffset = FVector(0, HalfGrid, -HalfGrid); break;
            case MoveDir::Backward: PivotOffset = FVector(0, -HalfGrid, -HalfGrid); break;
            }
            PivotWorld += PivotOffset;

            if (Result.moved)
            {
                DrawDebugSphere(GetWorld(), PivotWorld, 10.0f, 12, FColor::Blue, false, -1.0f, 1, 3.0f);
            }
            else
            {
                float Size = 10.0f;
                if (Dir == MoveDir::Left || Dir == MoveDir::Right)
                {
                    DrawDebugLine(GetWorld(), PivotWorld + FVector(-Size, 0, Size), PivotWorld + FVector(Size, 0, -Size), FColor::Red, false, -1.0f, 1, 3.0f);
                    DrawDebugLine(GetWorld(), PivotWorld + FVector(Size, 0, Size), PivotWorld + FVector(-Size, 0, -Size), FColor::Red, false, -1.0f, 1, 3.0f);
                }
                else
                {
                    DrawDebugLine(GetWorld(), PivotWorld + FVector(0, -Size, Size), PivotWorld + FVector(0, Size, -Size), FColor::Red, false, -1.0f, 1, 3.0f);
                    DrawDebugLine(GetWorld(), PivotWorld + FVector(0, Size, Size), PivotWorld + FVector(0, -Size, -Size), FColor::Red, false, -1.0f, 1, 3.0f);
                }
            }
        }
    }
}

const GameplayState& UUTLoxComponent::GetGameState() const { return State; }
const PieceAnimationState& UUTLoxComponent::GetAnimState() const { return AnimState; }

TArray<FCellInfo> UUTLoxComponent::GetAllCells() const
{
    TArray<FCellInfo> Result;
    const Level& L = State.level;
    for (int z = L.minZ; z < L.minZ + L.sizeZ; ++z)
        for (int y = L.minY; y < L.minY + L.sizeY; ++y)
            for (int x = L.minX; x < L.minX + L.sizeX; ++x)
            {
                CellType Type = GetCell(L, { x, y, z });
                if (Type == CellType::Empty) continue;
                FCellInfo Info;
                Info.X = x; Info.Y = y; Info.Z = z;
                Info.Type = static_cast<uint8>(Type);
                Result.Add(Info);
            }
    return Result;
}

FVector UUTLoxComponent::CellToWorld(int32 X, int32 Y, int32 Z) const
{
    return FVector(X * GridSpacingCm, Y * GridSpacingCm, Z * GridSpacingCm);
}

FVector UUTLoxComponent::GetLevelCenterWorld() const
{
    if (State.level.sizeX == 0) return FVector::ZeroVector;

    float CenterX = State.level.minX + (State.level.sizeX - 1) * 0.5f;
    float CenterY = State.level.minY + (State.level.sizeY - 1) * 0.5f;
    float CenterZ = State.level.minZ + (State.level.sizeZ - 1) * 0.5f;

    return CellToWorld(CenterX, CenterY, CenterZ);
}

void UUTLoxComponent::RotateCameraAroundCenter(AActor* CameraActor, float AngleDegrees) const
{
    if (!CameraActor || State.level.sizeX == 0) return;

    FVector LevelCenter = GetLevelCenterWorld();
    FVector CamPos = CameraActor->GetActorLocation();
    
    // Orbit the position around the Z axis
    FVector Offset = CamPos - LevelCenter;
    FVector RotatedOffset = Offset.RotateAngleAxis(AngleDegrees, FVector::UpVector);
    
    CameraActor->SetActorLocation(LevelCenter + RotatedOffset);
    
    // Add the yaw rotation so the camera orientation matches the orbit
    CameraActor->AddActorWorldRotation(FRotator(0.0f, AngleDegrees, 0.0f));
}

void UUTLoxComponent::GetPieceCellPositions(TArray<FVector>& OutPositions) const
{
    CellSet Cells;
    GetPieceCells(State, Cells);
    OutPositions.SetNum(kCellCount);
    for (int i = 0; i < kCellCount; ++i)
        OutPositions[i] = CellToWorld(Cells[i].x, Cells[i].y, Cells[i].z);
}

void UUTLoxComponent::GetAnimatedPieceTransforms(TArray<FPieceCellTransform>& OutTransforms) const
{
    OutTransforms.SetNum(kCellCount);

    if (!AnimState.active)
    {
        CellSet Cells;
        GetPieceCells(State, Cells);
        for (int i = 0; i < kCellCount; ++i)
        {
            OutTransforms[i].Position = CellToWorld(Cells[i].x, Cells[i].y, Cells[i].z);
            OutTransforms[i].Rotation = FRotator::ZeroRotator;
        }
        return;
    }

    if (AnimState.phase == AnimPhase::Rotating)
    {
        float T = 1 - FMath::Pow(1 - FMath::Clamp(AnimState.timer / AnimState.rotationDuration, 0.f, 1.f), 4);

        // Pivot is in cell space, convert to world
        FVector PivotWorld = CellToWorld(AnimState.pivotCell.x, AnimState.pivotCell.y, AnimState.pivotCell.z);

        // Figure out which axis we're rotating around from the move direction
        FVector RotAxis = FVector::ZeroVector;
        float RotSign = 1.f;
        FVector PivotOffset = FVector::ZeroVector;
        float HalfGrid = GridSpacingCm * 0.5f;

        switch (AnimState.dir)
        {
        case MoveDir::Left:     
            RotAxis = FVector(0, 1, 0); RotSign = -1.f; 
            PivotOffset = FVector(-HalfGrid, 0, -HalfGrid);
            break;
        case MoveDir::Right:    
            RotAxis = FVector(0, 1, 0); RotSign = 1.f; 
            PivotOffset = FVector(HalfGrid, 0, -HalfGrid);
            break;
        case MoveDir::Forward:  
            RotAxis = FVector(1, 0, 0); RotSign = -1.f; 
            PivotOffset = FVector(0, HalfGrid, -HalfGrid);
            break;
        case MoveDir::Backward: 
            RotAxis = FVector(1, 0, 0); RotSign = 1.f; 
            PivotOffset = FVector(0, -HalfGrid, -HalfGrid);
            break;
        }

        PivotWorld += PivotOffset;

        float Angle = FMath::Lerp(0.f, 90.f * RotSign, T);
        FQuat RotQuat = FQuat(RotAxis, FMath::DegreesToRadians(Angle));

        for (int i = 0; i < kCellCount; ++i)
        {
            FVector CellWorld = CellToWorld(AnimState.oldCells[i].x, AnimState.oldCells[i].y, AnimState.oldCells[i].z);
            // Rotate around pivot
            FVector Offset = CellWorld - PivotWorld;
            FVector Rotated = RotQuat.RotateVector(Offset);
            OutTransforms[i].Position = PivotWorld + Rotated;
            OutTransforms[i].Rotation = RotQuat.Rotator();
        }
    }
    else // AnimPhase::Gravity
    {
        // Gravity is just translation � keep the final rotation from the rotate phase
        float T = FMath::Clamp(AnimState.timer / AnimState.gravityDuration, 0.f, 1.f);
        for (int i = 0; i < kCellCount; ++i)
        {
            FVector From = CellToWorld(AnimState.rotatedCells[i].x, AnimState.rotatedCells[i].y, AnimState.rotatedCells[i].z);
            FVector To = CellToWorld(AnimState.finalCells[i].x, AnimState.finalCells[i].y, AnimState.finalCells[i].z);
            OutTransforms[i].Position = FMath::Lerp(From, To, T);
            OutTransforms[i].Rotation = FRotator::ZeroRotator; // snapped to grid by now
        }
    }
}