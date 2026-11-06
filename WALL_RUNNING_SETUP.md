# Agile Wall Running

Current project-wide startup and Windows build instructions are in
`MECHANICS_INTEGRATION.md`. Every project level uses the native companion setup
without resaving assets; the manual Blueprint workflow below is optional legacy setup.

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

- Add a tall, long cube wall with simple collision, **WorldStatic** or stationary
  **WorldDynamic** object type (no physics simulation),
  and collision that blocks **Pawn**. Floors should also have static collision.
  Use a wall roughly 1000 cm long and 400 cm high to start with.
- Move alongside the wall and jump. Continue moving along it while airborne.
  Wall detection reaches 40 cm beyond the capsule, requires 200 cm/s horizontal
  speed. There is no minimum height above the floor, but you must be airborne.
- A wall run moves at 650 cm/s with 20% of normal gravity for up to 1.5 seconds.
- Release and press Space again to jump away. Releasing movement, losing the wall,
  reaching the time limit, landing, or switching with Tab ends the run.
- Press jump during a wall run to launch across to an opposing wall while keeping
  forward momentum. Keep holding forward; catching the opposite wall automatically
  starts a fresh 1.5-second run. Release and press jump again for each transfer.
  There is no total limit on successful transfers, and no landing is required.
- The departure surface cannot be caught again during that transfer. Opposing
  faces can belong to the same mesh/actor. Corners do not refresh the run.
- A run that expires or ends without a wall jump cannot restart until landing.
  Releasing forward input, disabling the ability, switching characters, or missing
  the 1.2-second transfer window cancels the transfer. Landing resets eligibility.

### Tuning wall-to-wall transfers

In the character Blueprint's **Character Movement > Abilities > Wall Run**, tune
**Wall Jump Away Speed** (default 450 cm/s), **Wall Jump Up Speed** (500 cm/s),
and **Wall Transfer Duration** (1.2 seconds). Up speed is set on each jump rather
than added to existing upward velocity. Transfers use normal gravity and preserve
their forward/sideways launch momentum while forward input is held; ordinary air
steering resumes if the transfer is cancelled or expires. Each catch clamps
vertical speed using the existing wall-run entry rule.

Start with long parallel walls about **300–350 cm apart between their inner
faces**, at least 1000 cm tall, and hold forward while alternating jumps. Wider
gaps need a stronger sideways launch and/or more upward speed. Jump before the
individual run expires; each successful catch refreshes the timer. The chain has
no counter limit, but gap width and jump timing still determine whether the player
can keep their height. Wall detection remains local; it does not teleport or pull
the character across a gap. The existing left/right animation state updates on
each catch, without new input actions or animation wiring.

The movement supports static, nearly vertical walls and standard downward gravity.
There is no camera tilt or multiplayer prediction. Use the animation setup below
to replace the template falling pose during wall runs; it does not change physics.

## Agile Animation Wiring (UE 5.6)

The native playable character now selects
`/Game/Characters/Mannequins/Animations/ABP_Agile` for Agile. Strong still selects
`ABP_Manny`. Existing Blueprint subclasses with an overridden **Agile Animation
Class** must select `ABP_Agile` or reset that property to the native default.
Do not change `ABP_Manny`, `ABP_Quinn`, the skeletons, or the existing input graph.

Close the editor, rebuild C++, and reopen before looking for the new function.
Only the C++ connection is implemented here: the graph wiring must be saved on
the UE 5.6 PC. A copied, unwired `ABP_Agile` still plays normal locomotion.

1. Open `ABP_Agile`. Create two Boolean variables: `WallRunning` and `WallOnRight`,
   both default false.
2. In its **Event Graph**, use **Event Blueprint Update Animation**. Preserve any
   existing update logic, adding a **Sequence** if needed. Each update, use
   **Try Get Pawn Owner > Get Movement Component > Cast To
   CompanionCharacterMovementComponent**. Do not use Get Player Character: the
   animation must read its own pawn when characters switch or become AI-controlled.
3. From the successful cast, call **Is Wall Running** and **Is Wall On Right**.
   Set the two Boolean variables from those results. On an invalid owner or failed
   cast, set both false. Use this regular Event Graph, not a thread-safe animation
   update function, for the pawn/component calls.
4. In **AnimGraph**, drag in these retargeted sequences as Sequence Players:
   `/Game/Characters/Agile/Animations/Wallrun/Quinn_wall_run_left` and
   `/Game/Characters/Agile/Animations/Wallrun/Quinn_wall_run_right`.
   Enable **Loop Animation** on each. Keep **Enable Root Motion** off in both
   animation assets: the existing C++ movement owns character displacement.
5. Add **Blend Poses by bool**. Its **Active Value** is `WallOnRight`;
   **True Pose** is the right Sequence Player; **False Pose** is the left one.
   Set True/False Blend Time to `0.15` seconds. Name/comment it **Wall Side**.
6. Find **Output Pose**. Preserve the entire existing graph feeding it (including
   its normal foot IK). Insert a second **Blend Poses by bool** immediately before
   Output Pose: original graph output into **False Pose**, Wall Side output into
   **True Pose**, `WallRunning` into **Active Value**. Connect its result to Output
   Pose and set both blend times to `0.15` seconds. This keeps foot IK on normal
   locomotion without letting ground-foot correction distort the wall-run poses.
7. Compile and save `ABP_Agile` and both animation assets. Test Agile on a left and
   right wall, then run back in the opposite direction. The side is relative to
   travel along the wall, not the camera. Test landing, releasing movement,
   jumping away, and switching with Tab; each must blend out of the wall pose.
   Strong and the AI follower must retain normal walking/jumping animations.

The functions only expose movement state; they do not start a wall run or add
traces, inputs, impulses, montages, or mesh rotation. If the pose is reversed,
first check True/False pose connections and preview the two named animation clips.

## Verification

For live diagnosis, enter `companion.WallRun.Debug 1` in the editor's console
command field, then Play. The controlled companion shows the current blocker
(disabled ability, grounded, low speed, missing wall, or wrong movement direction).
Side traces turn green for a valid wall and red otherwise. `RUNNING` confirms the
physics is active; the dedicated pose also requires the animation wiring above.
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

The continuous-transfer extension adds `OpposingWallChain` and `TransferExits`.
The chain test makes twelve consecutive transfers from either starting side,
including two opposing walls on one actor and a stronger sideways launch. Exit
tests cover timeout, released input, disabled abilities, loss of possession, and
explicit cancellation. The exact movement/test sources compiled in an isolated
UE 5.3 compatibility project, and all twelve `MechanicTesting` native tests passed
with zero test warnings or failures. Only the temporary project's target settings
were adapted for 5.3; this checkout retains its UE 5.6 targets. Windows UE 5.6
compilation and visual/game-feel checks still need to run on the PC.

Also test in Play: both characters can jump, only the agile character can double
jump and wall run, a wall directly ahead does not trigger a run, running off a wall
end resumes falling, and repeated Tab switches retain normal movement and following.

The mechanics originated in the 5.3 prototype. The integration retains the 5.6
blockout's `/Game/Input/IMC_Default` mapping and updates both targets for 5.6.
The latest integration has not been compiled or played on Windows 5.6 here.
The animation connection is verified in an isolated UE 5.3 compatibility project,
without opening or resaving the UE 5.6 assets. Native tests also cover the exposed
animation state on both sides in both travel directions and its reset on exit.
The compatibility Editor build and all ten `MechanicTesting` automation tests
passed with zero test warnings or failures after this change.
Build and run the tests again on the university PC, then check the actual poses
and blend timing in Play; headless tests do not verify the Blueprint graph.
