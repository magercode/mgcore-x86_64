param(
  [string]$Version = "v10.x-binary",
  [string]$Destination = "tools/limine"
)

$destPath = Resolve-Path "." | ForEach-Object { Join-Path $_ $Destination }
$parent = Split-Path $destPath -Parent
if (-not (Test-Path $parent)) {
  New-Item -ItemType Directory -Force -Path $parent | Out-Null
}

if (Test-Path $destPath) {
  Write-Host "Limine directory already exists at $destPath"
  Write-Host "Remove it first if you want to re-clone a different version."
  exit 0
}

git clone https://codeberg.org/Limine/Limine.git --branch=$Version --depth=1 $destPath
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Limine binary release cloned to $destPath"
Write-Host "Re-run: cmake -S . -B build -G Ninja"
