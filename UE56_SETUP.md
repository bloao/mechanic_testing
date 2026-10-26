# Combined Blockout and Mechanics (UE 5.6)

`main` contains the teammate's `Content/ThirdPerson/Lvl_ThirdPerson.umap` blockout,
the companion switching/following controller, double jump, wall running, the smash
and breakable framework, and the wall-run automation tests. The 5.6 Content assets
and map settings are retained unchanged; Mac 5.3 Blueprints were not copied over them.
Adding C++ source does not automatically reparent or configure existing Blueprints.

## Windows Build

1. Use Unreal Engine 5.6, not 5.3 or 5.4. If asked to choose an engine, select the
   binary build installed at `C:\Program Files\Epic Games\UE_5.6`.
2. Work from a local drive. The previous university build treated a source path
   starting `//mfs04/...` as a compiler option and failed with D8003. Keep the network
   copy as a backup, not the active build folder.
3. Download LFS assets before opening the project. For a new local clone in PowerShell:

   ```powershell
   Set-Location $HOME
   git lfs install
   git clone --branch main --single-branch https://github.com/bloao/mechanic_testing.git mechanic_testing-main-ready
   Set-Location .\mechanic_testing-main-ready
   git lfs pull
   ```

   Use another new folder name if that destination exists. Do not delete old work.
4. Both `.Target.cs` files already use `BuildSettingsVersion.V5`,
   `EngineIncludeOrderVersion.Unreal5_6`, and `WindowsPlatform.bWriteSarif = false`.
   Do not reapply old 5.3 settings or add a unique/overridden build environment.
   SARIF output is disabled for the previous `/experimental:log` failure; normal
   compiler errors, warnings, and strict conformance remain enabled.
5. Open `mechanic_testing.uproject` and allow rebuilding, or build from the clone folder:

   ```powershell
   $ProjectFile = Join-Path (Get-Location).Path "mechanic_testing.uproject"
   & "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" mechanic_testingEditor Win64 Development "-Project=$ProjectFile" -WaitMutex -NoEngineChanges -NoHotReloadFromIDE
   ```

The university's MSVC 14.51 and SDK were detected, and a local-drive build reached
game code. The C4458 `Character` member-hiding errors are fixed by renaming the local
variables to `CompanionPawn`, not by disabling warnings. MSVC 14.51 is still not
UE 5.6's preferred toolchain; further compatibility errors may require IT to install
MSVC v143 14.38 and a Windows SDK. A successful complete Windows build is not yet verified.

## One-Time Blueprint Setup

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson` in the Content Browser. Keep the blockout.
2. Open `BP_ThirdPersonCharacter`. Use **File > Reparent Blueprint** to select
   `CompanionCharacterBase`, then compile and save. Its inherited Character Movement
   must be `CompanionCharacterMovementComponent`, with **Abilities > Wall Run** settings.
   Preserve the existing mesh, camera, and Enhanced Input events.
3. In the map's **World Settings**, check its GameMode Override. In that GameMode's
   Class Defaults, set **Player Controller Class** to `CompanionSwitchPlayerController`.
   A per-map override takes precedence over the project default GameMode.
4. Place two copies of the character Blueprint, keeping existing ones if suitable.
   For each, set **Auto Possess Player = Disabled**, **Auto Possess AI = Placed in World
   or Spawned**, **AI Controller Class = AIController**, and add the Actor tag
   `CompanionCharacter`. Do not add extra tagged characters unless intended.
5. Add or retain a Nav Mesh Bounds Volume covering the floor. Press `P` to check green
   navigation coverage. Walls must block Pawn and use WorldStatic collision for wall running.
6. Keep the character's movement and jump Enhanced Input events. Jump presses call
   `Jump`; release/cancellation calls `Stop Jumping`. The controller adds
   `/Game/Input/IMC_Default` for this 5.6 template. Tab switching uses its existing key setting.
7. The controller's `Agile Companion Index` (default 0) gives one character both double
   jump and wall running; the other gets one jump. Outliner names alone do not assign
   the role. Use the controller's explicit companion pair if stable ordering is needed.

The smash framework still requires the strong character's Blueprint to enable
`Can Smash`, assign its Smash Input Action, and map that action. Breakables expose
hit count, hide/collision, and destruction settings. No wall-run animation, box
pushing, or new animation assets are included.

## Test Before Sharing Changes

- Build successfully with 5.6, then verify the map, meshes, and materials load.
- Check movement and normal jumping for both characters; double jump and wall run only for agile.
- Jump while moving close and parallel to a static wall. No minimum ground clearance is required.
- Test both wall sides, jumping off, landing, timeout, and repeated Tab switching and AI following.
- Run `MechanicTesting.WallRun` in Session Frontend Automation. Earlier native 5.3 tests
  passed, but this final 5.6 integration and its Blueprint wiring must be tested on the PC.
- Use `companion.WallRun.Debug 1` to inspect live status, then `companion.WallRun.Debug 0`.
  The template falling animation remains until wall-run animations are added separately.
- If compilation fails, collect the current log, not only the generic rebuild popup:

  ```powershell
  Get-Content "$env:LOCALAPPDATA\UnrealBuildTool\Log.txt" -Tail 80
  ```

For an existing clone with local changes, back up or commit those changes before
pulling. Never reset it or overwrite assets just to make a pull succeed. Generated
`Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` folders are not committed.
