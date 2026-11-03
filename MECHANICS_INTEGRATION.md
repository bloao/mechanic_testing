# Complete mechanics in both gameplay levels

The integrated gameplay levels are `/Game/ThirdPerson/Lvl_ThirdPerson` and
`/Game/ThirdPerson/LVL_level2`. Their map packages, external actors, meshes,
materials, transforms, collision settings and gravity settings are preserved.
The mechanics setup happens in the play session rather than resaving the maps.

## Update and build on Windows

Save and close Unreal Editor, then run from your existing local project folder:

```powershell
git pull --ff-only origin main
git lfs pull
powershell -NoProfile -ExecutionPolicy Bypass -File .\Scripts\build-windows.ps1
```

Reopen `mechanic_testing.uproject` with UE 5.6 only after `Result: Succeeded`.
The script includes `-NoUBA -NoUBALocal` and stops if Unreal Editor is running,
avoiding the known DLL replacement failure. It does not kill editor processes or
discard changes. Pass `-EngineRoot` for a different UE 5.6 installation location.

## Shared level startup

`CompanionGameInstance` chooses `CompanionGameMode` for both named gameplay levels,
including their Play In Editor copies. A saved template GameMode override cannot
silently replace the native playable character or companion controller. Other map
names retain their selected GameMode. The existing meshes, animation Blueprints,
input assets and per-level gravity are still used.

The GameMode spawns Agile and Strong near PlayerStart. It checks additional
directions and closer positions when geometry blocks the preferred spawn spot,
without forcing the capsule inside obstacles. Agile's abilities are configured
even if Strong cannot spawn. Old placed pawns cannot automatically steal player
input at startup; their transforms and visibility are retained.

## Included mechanics

- WASD movement, mouse look, Space jump, Tab switching and AI following.
- Agile double jump, left/right wall runs, wall jumps, opposing-wall catches and
  unlimited successful wall-run transfers. Release and press Space per transfer.
- Strong single jump, E smash, and the `BreakableActor`/smashable-interface framework.
- Two-way `CompanionJumpNavLink` jumps/drops, Agile AI double jumps, default-on
  attempts when a predicted arc fails, and facing along jump/drop travel.
- Role restrictions, collision-safe spawning, landing/timeout/possession cleanup,
  and wall-run/follower diagnostics.

Wall detection accepts Pawn-blocking, nearly vertical `WorldStatic` and stationary
`WorldDynamic` surfaces, including stationary Blueprint blockout geometry.
Simulating or moving walls are excluded. Geometry must have usable collision;
this code does not create collision on decorative meshes or convert arbitrary
props into breakable objects. Box pushing remains unimplemented.

## Navigation and routes

Existing NavMesh bounds and jump links are retained. A level with no bounds gets
a transient bounds volume covering its navigation-relevant collision geometry;
dynamic Recast generation builds walking navigation during Play. This does not
save objects, volumes or jump routes into a map. Navigation may need a moment to
build after Play begins.

Place `CompanionJumpNavLink` endpoints on the two walkable surfaces for each
designed platform route. A `Both Ways` link supports jumping up and dropping down.
The fallback NavMesh does not invent jump routes between arbitrary ledges. For an
editor preview while designing routes, use authored NavMesh bounds and Build Paths
as described in `SMART_JUMP_LINK_SETUP.md`.

## Confirm the active version

Test each level with Play, rather than Simulate. Runtime actors should be labeled
Agile and Strong. Output Log prints `Companion mechanics active in ...` with the
native character/controller classes. Test Agile's double jump, opposing-wall
transfers while holding forward, Tab switching, Strong's E smash on a configured
breakable, following, and both directions of the placed jump links.

Use `companion.WallRun.Debug 1` for wall/transfer status and
`companion.Follow.Debug 1` for route status. Set either to `0` afterward.
If Strong cannot spawn, a warning identifies missing clear floor around
PlayerStart; Agile can still use its movement abilities.

## Validation scope

The native compatibility build and all **16** `MechanicTesting` automation tests
passed with zero test warnings or failures for this integration.

The integration tests exercise both gameplay map names and PIE names through
Unreal's GameMode creation path, deliberately overriding a legacy GameMode. They
also build a walkable runtime NavMesh, check reuse of authored navigation bounds,
preserve physical transforms/collision, and create the pair on a small Blueprint-style platform.
Continuous-run tests cover static and Blueprint-style dynamic walls. Existing
smash, switching, following, jump/drop and facing regressions remain included.

This Mac has UE 5.3, so native code is compiled/tested in an isolated compatibility
project; the checkout retains UE 5.6 targets. The UE 5.6 map packages were inspected
read-only and were not opened or resaved with UE 5.3. Actual level visuals, route
coverage, collision geometry and animation blending still require the UE 5.6
Windows play test above.
