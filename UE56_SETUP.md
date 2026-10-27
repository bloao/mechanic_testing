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
variables to `CompanionPawn`, not by disabling warnings. After the test-context fix,
the user confirmed a successful Windows 5.6 build with MSVC 14.51 and SDK 10.0.26100.0.
MSVC 14.51 is still not UE 5.6's preferred toolchain. The new native pair-spawning
setup must be rebuilt and tested on the PC; that newer change is not yet verified there.

## Two-Character Setup

Use `TWO_CHARACTERS_SETUP.md` for the native pair setup:

1. Open `/Game/ThirdPerson/Lvl_ThirdPerson` and set **World Settings > GameMode Override**
   to **CompanionGameMode**. Save the map in 5.6. The old per-map Blueprint override
   otherwise takes precedence over the new project default.
2. Keep PlayerStart on clear floor and add/retain a Nav Mesh Bounds Volume.
3. Press Play. The GameMode spawns Agile (Quinn) and Strong (Manny), assigns abilities
   and AI controllers, and preserves the existing Tab switch/follow logic.
4. Space jumps; Agile can double jump and wall run. E smashes only as Strong.

No reparenting or manual character placement is needed for this GameMode. The older
Blueprint character and GameMode assets remain unchanged for reference or a custom
Blueprint workflow. No wall-run/smash animations or box pushing are included.

## Test Before Sharing Changes

- Build successfully with 5.6, then verify the map, meshes, and materials load.
- Check movement and normal jumping for both characters; double jump and wall run only for agile.
- Jump while moving close and parallel to a static wall. No minimum ground clearance is required.
- Test both wall sides, jumping off, landing, timeout, and repeated Tab switching and AI following.
- Run `MechanicTesting.WallRun` and `MechanicTesting.Companions` in Session Frontend
  Automation. Also verify the actual 5.6 map, input bindings, and appearance in Play.
- Use `companion.WallRun.Debug 1` to inspect live status, then `companion.WallRun.Debug 0`.
  The template falling animation remains until wall-run animations are added separately.
- If compilation fails, collect the current log, not only the generic rebuild popup:

  ```powershell
  Get-Content "$env:LOCALAPPDATA\UnrealBuildTool\Log.txt" -Tail 80
  ```

For an existing clone with local changes, back up or commit those changes before
pulling. Never reset it or overwrite assets just to make a pull succeed. Generated
`Binaries`, `Intermediate`, `Saved`, and `DerivedDataCache` folders are not committed.
