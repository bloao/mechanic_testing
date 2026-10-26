# Upload an Unreal Environment to `basic-blockout`

This guide is for teammates using **Windows PowerShell** and Unreal Engine 5.6. It imports an environment into the existing project without replacing the player mechanics.

## Repository facts

- Repository: `https://github.com/bloao/mechanic_testing.git`
- Target branch: `basic-blockout` (confirmed on GitHub)
- Project file: `mechanic_testing.uproject`
- Project content folder: `Content`
- Author's current local project: `/Users/bloao/Documents/Unreal Projects/mechanic_testing`
- Git remote: `origin` points to the repository above.
- Git LFS is configured in `.gitattributes` for `*.uasset` and `*.umap`.
- `Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, IDE folders, and generated project files are ignored.

> **Version warning:** `main` and `basic-blockout` record Unreal Engine 5.6 and include assets saved in 5.6. Do not open or resave these branches in 5.3. Use `main` for the combined mechanics and blockout; `basic-blockout` remains the separate environment upload branch. Build from a local drive, not network-backed Documents; see `UE56_SETUP.md` for the Windows setup.

## Numbered checklist

1. **Close Unreal Editor, open Windows PowerShell, and check that Git and Git LFS are installed.**

   ```powershell
   git --version
   git lfs version
   git lfs install
   ```

   If `git lfs` is not recognised, install it, reopen PowerShell, and run `git lfs install` again:

   ```powershell
   winget install GitHub.GitLFS
   ```

2. **Choose where the separate clone will be created.** Replace the example parent folder if necessary. Do not clone inside the teammate's original Unreal project.

   ```powershell
   Set-Location "$HOME\Documents"
   Test-Path ".\mechanic_testing-basic-blockout"
   ```

   `False` means the folder name is available. If it returns `True`, use a different new folder name such as `mechanic_testing-basic-blockout-ALEX`; do not delete an existing project unless its owner has confirmed it is disposable.

3. **Clone only the `basic-blockout` branch into its own folder.** This is the exact standard clone command:

   ```powershell
   git clone --branch basic-blockout --single-branch https://github.com/bloao/mechanic_testing.git mechanic_testing-basic-blockout
   Set-Location ".\mechanic_testing-basic-blockout"
   git lfs pull
   ```

   If a different clone folder name was chosen in step 2, replace `mechanic_testing-basic-blockout` in both commands.

4. **Verify the repository, remote, and current branch.**

   ```powershell
   git remote -v
   git branch --show-current
   git status
   ```

   The branch must say `basic-blockout`. If it does not, switch safely:

   ```powershell
   git fetch origin
   git switch basic-blockout
   git pull --ff-only origin basic-blockout
   ```

5. **Pull the newest team changes before starting.** Every teammate must do this before each new piece of work.

   ```powershell
   git pull --ff-only origin basic-blockout
   git lfs pull
   ```

6. **Record the cloned project's exact `Content` destination.** This is the folder Unreal's Migrate window must receive.

   ```powershell
   $ClonePath = (Get-Location).Path
   $ContentPath = Join-Path $ClonePath "Content"
   $ContentPath
   Test-Path (Join-Path $ClonePath "mechanic_testing.uproject")
   Test-Path $ContentPath
   ```

   Both `Test-Path` commands should return `True`. Keep the printed `$ContentPath` available.

7. **Prepare a unique folder in the teammate's original Unreal project.** In its Content Browser, create something like `Content/Environments/<TEAMMATE_NAME>` and move that teammate's map and environment-specific assets into it where practical. Fix redirectors if Unreal requests it. Unique folders reduce binary asset conflicts.

8. **Migrate the map using Unreal Engine, not File Explorer.** In the teammate's original project:

   1. Select the environment map in the **Content Browser**.
   2. Right-click it and choose **Asset Actions → Migrate**.
   3. Review the Asset Report and keep the required dependencies selected.
   4. Select the cloned project's exact `Content` folder printed in step 6.
   5. Confirm the migration.
   6. If Unreal reports files that already exist, choose **No/Skip** and speak to the team. Never overwrite existing mechanics, maps, or shared assets without agreement.

   Migration **copies** the map and dependencies. It does not delete or move the original project.

9. **Open the cloned `mechanic_testing.uproject` in Unreal Engine 5.6 and test it.** Confirm that the migrated map opens, its materials are present, and the existing mechanics still work. Save only the migrated environment/map changes. Keep the project in 5.6; do not independently convert it to another engine version.

10. **Close Unreal Editor and review exactly what changed.**

    ```powershell
    Set-Location $ClonePath
    git branch --show-current
    git status --short
    git lfs status
    ```

    Expected changes should be environment files under `Content`. Stop if existing player mechanics, `Source`, `Config`, or an unrelated map changed unexpectedly.

11. **Pull again before committing, because another teammate may have uploaded while the environment was being migrated.**

    ```powershell
    git pull --ff-only origin basic-blockout
    ```

    If this refuses because of local changes, do not discard or overwrite anything. Commit the environment first in step 12, then use the rebase command in step 13.

12. **Stage only that teammate's environment, inspect the staged list, and commit.** Replace `ALEX` with the exact unique folder name created in step 7. The conditional commands also include World Partition's external actor/object data when those folders exist.

    ```powershell
    $EnvironmentName = "ALEX"
    $EnvironmentPaths = @(
        "Content/Environments/$EnvironmentName",
        "Content/__ExternalActors__/Environments/$EnvironmentName",
        "Content/__ExternalObjects__/Environments/$EnvironmentName"
    )
    $EnvironmentPaths | Where-Object { Test-Path $_ } | ForEach-Object { git add -- $_ }
    git status --short
    git diff --cached --stat
    git commit -m "Add $EnvironmentName environment blockout"
    ```

    Check `git status --short` carefully. Staged lines appear without `?` in the left column. Do not commit if `Source`, `Config`, `BP_ThirdPersonCharacter`, `ThirdPersonMap`, or another teammate's environment appears unexpectedly. Do not use `git add --all` or `git add -- Content` for this task because either could include unrelated project or mechanics changes.

13. **Pull the latest branch again before pushing.** This is essential when several teammates share the branch.

    ```powershell
    git pull --rebase origin basic-blockout
    git status
    ```

    If Git reports a conflict in a `.uasset` or `.umap`, do not choose one version blindly. Abort and coordinate with the teammate who changed the same asset:

    ```powershell
    git rebase --abort
    ```

14. **Push the environment commit and verify the result.**

    ```powershell
    git push origin basic-blockout
    git status
    git log -1 --oneline
    ```

    A successful final status should say the branch is up to date and the working tree is clean.

15. **Use these fixes for common errors.**

    - **Destination folder already exists:** Choose another new clone folder name and update the folder name in steps 2 and 3. Do not clone over an existing project.
    - **`git lfs` is not recognised:** Install Git LFS with the step 1 command, reopen PowerShell, run `git lfs install`, then clone again into a new folder.
    - **Wrong branch:** Repeat step 4. Do not migrate or commit until `git branch --show-current` reports `basic-blockout`.
    - **GitHub authentication fails:** Confirm the teammate has repository access. Complete the Git Credential Manager browser sign-in when prompted; GitHub account passwords are not accepted as Git passwords.
    - **Push is rejected:** Another teammate probably pushed first. Repeat step 13, test the result, and retry step 14. Never force-push.
    - **Binary asset conflict:** Run the `git rebase --abort` command from step 13 and agree which map/asset should be kept. Unreal binary assets cannot be safely text-merged.
    - **Migration targets the wrong folder:** Cancel it. The selected destination must end exactly in `mechanic_testing-basic-blockout\Content` (or the alternative clone name chosen in step 2).

16. **Never commit generated Unreal files.** Do not stage or upload:

    - `Binaries/`
    - `DerivedDataCache/`
    - `Intermediate/`
    - `Saved/`
    - `.vs/`, `.vscode/`, `.idea/`
    - Generated `.sln`, `.xcodeproj`, or `.xcworkspace` files

    The repository's `.gitignore` already excludes them. Keep the existing `Content`, `Config`, `Source`, `.uproject`, `.gitignore`, and `.gitattributes` files intact.
