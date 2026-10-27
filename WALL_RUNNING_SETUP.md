# Agile Wall Running

This prototype uses the existing companion controller and Third Person character.
The controller gives double jump and wall running to the same companion selected
by `Agile Companion Index` (default `0`). The other companion retains one jump.
Discovery order is not an explicit character identity: check which character has
the agile abilities in Play, or supply the controller's explicit companion pair.

## Rebuild and Blueprint Setup

`main` is the UE 5.6 integration with the teammate's original blockout assets
preserved. Start with `UE56_SETUP.md` on the university PC. The Mac build script
below belongs to the separate 5.3 prototype; it cannot build this 5.6 checkout.
For the new automatically spawned pair, use `TWO_CHARACTERS_SETUP.md` instead of
the manual Blueprint placement/reparenting workflow below.

1. Close Unreal Editor. Rebuild the `mechanic_testingEditor` target for your engine
   version. Restart the editor after the build; Live Coding alone is insufficient
   for this change to the native movement-component class.
   On this Mac, run the saved build script from Terminal:

   ```sh
   bash "/Users/bloao/Documents/Unreal Projects/mechanic_testing/Scripts/build-mac-local.sh"
   ```

   It selects the existing Clang 14/macOS 13.3 SDK setup without changing the system
   Xcode selection. Its default location is `/private/tmp/ue53-xcode-shim`.
   If that temporary directory is removed, the local compiler setup must be restored.
2. Open `BP_ThirdPersonCharacter` and check **Class Settings > Parent Class** is
   `CompanionCharacterBase`. The local Mac Blueprint already has this parent.
   Compile and save the Blueprint after rebuilding.
3. Select its **Character Movement** component. Its class should now be
   `CompanionCharacterMovementComponent`. Settings are under **Abilities > Wall Run**.
   The companion controller enables wall running only for the agile character.
4. Keep the existing jump and movement Enhanced Input bindings. No new Input Action
   or Mapping Context is required. The jump action must call `Jump` while pressed
   and `Stop Jumping` when released (including cancellation if configured).
5. Keep the existing companion GameMode/controller, two characters tagged
   `CompanionCharacter`, AI possession, and NavMesh setup. AI followers never wall run.

## Test Course and Controls

- Add a tall, long cube wall with simple collision, **WorldStatic** object type,
  and collision that blocks **Pawn**. Floors should also have static collision.
  Use a wall roughly 1000 cm long and 400 cm high to start with.
- Move alongside the wall and jump. Continue moving along it while airborne.
  Wall detection reaches 40 cm beyond the capsule, requires 200 cm/s horizontal
  speed. There is no minimum height above the floor, but you must be airborne.
- A wall run moves at 650 cm/s with 20% of normal gravity for up to 1.5 seconds.
- Release and press Space again to jump away. Releasing movement, losing the wall,
  reaching the time limit, landing, or switching with Tab ends the run.
- Only one wall run is allowed per airborne period. Landing restores it.

The first version supports static, nearly vertical walls and standard downward
gravity. It has no dedicated wall-run animation, camera tilt, or multiplayer
prediction. The template's existing falling animation remains in use.

## Verification

For live diagnosis, enter `companion.WallRun.Debug 1` in the editor's console
command field, then Play. The controlled companion shows the current blocker
(disabled ability, grounded, low speed, missing wall, or wrong movement direction).
Side traces turn green for a valid wall and red otherwise. `RUNNING` confirms the
physics is active even though the character still uses the template falling pose.
Enter `companion.WallRun.Debug 0` to disable diagnostics. If no message appears,
check Output Log for a missing custom movement component or character-parent warning.
These commands do not change or save the map or Blueprint assets.

UnrealHeaderTool passed with warnings treated as errors. An initial build selected
the system's Xcode 27 and failed SDK validation. Earlier successful builds used a
separate Clang 14 compiler and macOS 13.3 SDK, which are still installed. The saved
build script selects that existing setup. Rebuild before trying the new behavior.
The earlier 5.3 Editor target compiled and linked successfully through this script.
Before this integration, all three `MechanicTesting.WallRun` automation tests passed in headless game mode:
eligibility, jump/possession handling, and duration/landing recovery. The report is
in `Saved/Automation/WallRun/index.json`. Editor-mode headless runs encountered
engine startup issues, so the successful run used `-game -nullrhi` with the
unrelated Fab and Bridge plugins disabled for that process only.

In Unreal's Session Frontend Automation tab, run `MechanicTesting.WallRun`.
The tests create collision geometry without depending on project assets and cover
both wall sides, character eligibility, AI control, jump-off, possession changes,
duration limits, and recovery after landing.

Also test in Play: both characters can jump, only the agile character can double
jump and wall run, a wall directly ahead does not trigger a run, running off a wall
end resumes falling, and repeated Tab switches retain normal movement and following.

The mechanics originated in the 5.3 prototype. The integration retains the 5.6
blockout's `/Game/Input/IMC_Default` mapping and updates both targets for 5.6.
The latest integration has not been compiled or played on Windows 5.6 here.
The temporary Mac compiler setup is no longer available, so a new Mac build
could not be run either. Build and run the tests again on the university PC.
