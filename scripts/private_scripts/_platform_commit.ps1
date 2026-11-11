param(
    [Parameter(Mandatory = $true)]
    [string]$Tag,      # e.g. 2.0.1
    [Parameter(Mandatory = $true)]
    [string]$Platform  # path to module\re\platform
)

# Find the platform-source commit that belongs to an output tag.
# The platform repo itself has no 2.x tags, but its team bumps the
# basiclibrary_output submodule pointer in lockstep with "update <ver>"
# commit messages. Two independent methods, both must agree on nothing
# being found before we give up:
#   1) submodule-pin archaeology: the platform commit whose recorded
#      basiclibrary_output pointer equals the tag's commit
#   2) commit-message search: --grep "update <ver>"
$ErrorActionPreference = 'SilentlyContinue'

$basic = Join-Path $Platform 'basiclibrary_output'
$tagCommit = git -C $basic rev-parse "$Tag^{commit}" 2>$null
if (-not $tagCommit -or $LASTEXITCODE -ne 0) {
    Write-Output "ERROR: tag $Tag not found in basiclibrary_output"
    exit 1
}

# method 1: walk the commits that touched the submodule path
$found = $null
$commits = git -C $Platform log --format='%H' -n 50 -- basiclibrary_output 2>$null
foreach ($c in $commits) {
    $line = git -C $Platform ls-tree $c basiclibrary_output 2>$null
    if ($line -match "$tagCommit") { $found = $c; break }
}

# method 2 (cross-check / fallback): commit message, restricted to a's
# develop. Matching on --all can surface a commit from another branch
# (e.g. a master merge), and a must never be moved to a wrong commit.
$grep = $null
$devRef = git -C $Platform rev-parse --verify --quiet develop 2>$null
if (-not $devRef) { $devRef = 'origin/develop' }
$gmatches = git -C $Platform log $devRef --format='%H' -i --grep="update $Tag" -n 5 2>$null
foreach ($g in $gmatches) { $grep = $g; break }

if ($found) {
    $short = $found.Substring(0, 7)
    if ($grep -and $grep -ne $found) {
        Write-Output "WARN: pin-archaeology says $short but grep says $($grep.Substring(0,7)) - using pin"
    }
    Write-Output "OK $found"
    exit 0
}
if ($grep) {
    Write-Output "OK $grep"
    Write-Output "WARN: found via commit message only (no pin match)"
    exit 0
}
Write-Output "ERROR: no platform commit corresponds to tag $Tag"
exit 1
