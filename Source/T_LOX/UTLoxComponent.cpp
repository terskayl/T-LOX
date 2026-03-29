#include "UTLoxComponent.h"
#include "DrawDebugHelpers.h"

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
}

void UUTLoxComponent::RequestMove(MoveDir Dir)
{
    if (AnimState.active) return;
    MoveResult Result = TryMove(State, Dir);
    BeginMoveAnimation(AnimState, Result, Dir);
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
}

bool UUTLoxComponent::IsAnimating() const { return AnimState.active; }
bool UUTLoxComponent::IsGoalMet()   const { return IsGoalSatisfied(State); }
int32 UUTLoxComponent::GetMoveCount() const { return State.moveCount; }

void UUTLoxComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateAnimation(AnimState, DeltaTime);

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
        float T = FMath::Clamp(AnimState.timer / AnimState.rotationDuration, 0.f, 1.f);

        // Pivot is in cell space � convert to world
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