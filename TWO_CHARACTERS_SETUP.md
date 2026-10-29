# Two Characters in the Blockout (UE 5.6)

The new `CompanionGameMode` spawns the pair at Play, without changing any map assets
or needing to reparent the template Blueprint. Both use the existing mannequin
meshes and animations, the shared ability base, and the existing companion controller.

## One-Time Setup

1. Close Unreal and build the updated project with UE 5.6. Reopen it after compiling.
2. Open the teammate's `/Game/ThirdPerson/Lvl_ThirdPerson` blockout.
3. Open **Window > World Settings**. Set **GameMode Override** to **CompanionGameMode**.
   This is essential: the map's old Blueprint override takes precedence over the
   new project default. Save the map on your PC in 5.6.
4. Keep one PlayerStart with clear, flat floor around it (at least 250 cm on a side).
   Do not place two more template characters: the GameMode spawns the pair for you.
   If you previously placed test characters, disable **Auto Possess Player** on them.
   Existing actors are not deleted or hidden by this setup.
5. Make sure a **Nav Mesh Bounds Volume** covers the playable floor. Press **P** to
   check the green walkable area. AI following needs navigation; character switching
   and abilities can be tested even before navigation is built.
6. Press **Play**, not Simulate. In PIE's Outliner, the runtime actors are labeled
   **Agile** and **Strong**. Stopping Play removes these runtime actors; they spawn
   again next time. They will not appear as placed actors while editing the map.

## Controls and Roles

- **WASD / left stick:** move. **Mouse / right stick:** look.
- **Space:** jump. Release and press again in the air for Agile's second jump.
- **Tab:** switch. The inactive character receives an AIController and follows.
- **Agile (Quinn):** two jumps and wall running; cannot smash.
- **Strong (Manny):** one jump and smash; cannot wall run.
- **E:** Strong's prototype smash key. It does nothing as Agile. If a Blueprint
  assigns the existing Smash Input Action, bind/map that Enhanced Input action instead.
- To wall run, jump while moving close and parallel to a tall static wall. A second
  jump press while wall running pushes away. There is no ground-height restriction.

## Test a Breakable

Create a Blueprint child of the existing `BreakableActor` C++ class. Assign a static
mesh to its **Mesh** component, with collision enabled and an object type such as
WorldStatic or WorldDynamic. Place it within Strong's forward smash range (default
180 cm, sweep radius 90 cm). Press E facing it. Hits Required, hide/collision response,
and optional destruction remain Blueprint editable. No smash animation is supplied.

## Follower Walking Animation

The shared movement component enables acceleration-based AI path following. This
supplies the acceleration used by the template locomotion animation, instead of
moving the follower by direct velocity while its animation remains idle. Rebuild
after updating this C++ code, and test both followers after switching.

The follow refresh also preserves an active, waiting, or paused move request rather
than aborting and restarting it every 0.25 seconds. MoveToActor tracks a moving ground target;
the controller retries when idle and still stops within the configured distance.
Test sustained following and drop-link traversal in the 5.6 map after rebuilding.

## Following During Player Jumps

No extra Blueprint nodes or input mappings are required. The existing controller
traces below the leader's feet and projects a walkable hit onto the follower's
NavMesh. A single invisible, transient TargetPoint follows that ground position,
so the AI can keep moving while the player is airborne. If the trace or projection
fails, the last valid ground target is retained instead of chasing an air position.
Before any valid target exists, no new move is issued.

**Follow Ground Search Depth** defaults to 2000 cm and is editable on a controller
Blueprint child. The cache resets on character switching, and the target is cleaned
up with the controller. The configured stopping distance is measured against the
ground target, not the jumping player's capsule. The target has no collision.

Test on the PC: move and jump across a flat green NavMesh while the companion is
several metres behind; repeat with both characters. Also test jumps in place,
double jumps, landing on platforms, jumping over gaps, switching while airborne,
and Stop/Play. Following still requires valid NavMesh routes; this change does not
add AI jumping, climbing, or any new navigation links.

For manually placed two-way jump routes, see [Smart Jump Link Setup](SMART_JUMP_LINK_SETUP.md).
The optional `CompanionJumpNavLink` actor provides bounded normal jumps and landing
handoff; existing links and map assets are not replaced automatically.

## Settings and Limitations

Make a Blueprint child of `CompanionGameMode` to adjust Strong Companion Class or
Companion Spawn Distance. A Blueprint child of `CompanionPlayableCharacter` can
change its meshes, animation classes, smash key, camera boom, jump count, wall-run
settings, and smash parameters. Set the GameMode's Default Pawn Class and Strong
Companion Class to your custom character children as needed.

The controller assigns jump counts when it sets up the pair. To change Agile's
count, make a Blueprint child of `CompanionSwitchPlayerController`, edit **Agile
Jump Max Count**, and select it as the GameMode's Player Controller Class. The same
controller exposes Follow Acceptance Radius (250 cm), refresh interval, and Tab key.

Roles are assigned explicitly, not from Outliner names or actor discovery order.
The pair setup is repeatable without spawning duplicates, and switching cleans up
the incoming follower's old AI controller. Spawning fails safely with a log message
if all four candidate floor positions are blocked or absent; move PlayerStart or
adjust the spawn distance rather than enabling Always Spawn inside walls.

This remains a single-player prototype. Box pushing, wall-run/smash animations,
camera effects, and character-specific artwork are not included. The standard
template animations remain in use. The 5.6 meshes and input assets are referenced
by path but have not been resaved or edited on the Mac.

Run `MechanicTesting.WallRun` and `MechanicTesting.Companions` in Session Frontend
Automation. Also test in the 5.6 editor: both visible models and cameras, all inputs,
two jumps only for Agile, wall runs on both sides, only Strong smashing, 20 repeated
Tab switches, NavMesh following, and Stop/Play spawning only one pair each time.

## Verification

The native code passed UnrealHeaderTool and a Development Editor build in an
isolated UE 5.3 compatibility project. All nine `MechanicTesting` automation tests
passed: three wall-run regressions plus pair/switching/smash, no-floor spawning,
follower acceleration for both roles, airborne ground-target following, and smart
jump-link planning/traversal.
The pair test checks 20 switches, exactly one follower AI controller, role-specific
abilities, reuse without duplicate characters, and breakable hide/collision state.
It uses primitive collision to avoid asynchronous asset compilation in a headless run.
The airborne-follow test checks floor/platform traces, horizontal movement while
airborne, bounded search depth, last-target retention, switching, cleanup, and
preserving active/paused requests while still stopping at the ground target.
It seeds a cached target and synthetic path because the fixture has no baked
NavMesh. Actual NavMesh projection and path replanning need the PC Play test above.
Jump-link tests cover physical jumps in both directions, normal jump limits,
blocked/unreachable routes, landing handoff, possession changes, timeout, cleanup,
and preserving a paused traversal even when near the follow target.

Only the verification copy used 5.3 target settings and the legacy input mapping
path. This checkout retains its 5.6 targets and input paths. The actual Windows 5.6
build, native camera/input lifecycle, mannequin animation Blueprints, map override,
and NavMesh following still require the Editor checks above. The headless test
verifies acceleration, not the actual
animation Blueprint or the map's navigation route. No 5.6 assets were
opened or resaved with 5.3.
