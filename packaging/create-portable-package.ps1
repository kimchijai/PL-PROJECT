param(
    [string]$SourceDatabase = "",
    [string]$OutputZip = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-RepositoryPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

if ([string]::IsNullOrWhiteSpace($SourceDatabase)) {
    $SourceDatabase = Join-Path $repositoryRoot "bakery_pos.db"
}
$SourceDatabase = Resolve-RepositoryPath $SourceDatabase
if ([string]::IsNullOrWhiteSpace($OutputZip)) {
    $OutputZip = Join-Path $repositoryRoot "dist\CrombiCookiesPOS-Portable.zip"
}
$OutputZip = Resolve-RepositoryPath $OutputZip

$releaseExecutable = Join-Path $repositoryRoot "build\release\bakery_pos_gui.exe"
$qtBin = "C:\Qt\6.12.0\mingw_64\bin"
$deployTool = Join-Path $qtBin "windeployqt.exe"
$sqliteCli = "C:\msys64\ucrt64\bin\sqlite3.exe"
$sqliteDll = "C:\msys64\ucrt64\bin\libsqlite3-0.dll"
$mingwRuntime = "C:\Qt\Tools\mingw1310_64\bin"
$stageParent = Join-Path $repositoryRoot "build\portable-package"
$stageRoot = Join-Path $stageParent "CrombiCookiesPOS"
$stagedDatabase = Join-Path $stageRoot "data\bakery_pos.db"
$workspaceUploads = Join-Path $repositoryRoot "assets\products\uploads"
$localAppData = [Environment]::GetFolderPath(
    [Environment+SpecialFolder]::LocalApplicationData)
$userUploads = Join-Path $localAppData `
    "Crombi Cookies\Crombi Bakery POS\assets\products\uploads"

foreach ($requiredPath in @(
        $SourceDatabase,
        $releaseExecutable,
        $deployTool,
        $sqliteCli,
        $sqliteDll,
        (Join-Path $repositoryRoot "assets\fonts\Inter-Variable.ttf"),
        (Join-Path $repositoryRoot "assets\fonts\Poppins-Regular.ttf"),
        (Join-Path $repositoryRoot "assets\fonts\Poppins-SemiBold.ttf"))) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required packaging input is missing: $requiredPath"
    }
}
if (Test-Path -LiteralPath $stageRoot) {
    throw "Package staging directory already exists; preserve it or remove it explicitly: $stageRoot"
}
if (Test-Path -LiteralPath $OutputZip) {
    throw "Package ZIP already exists; it will not be overwritten: $OutputZip"
}

$sourceIntegrity = (& $sqliteCli -readonly $SourceDatabase "PRAGMA integrity_check;") -join ""
if ($LASTEXITCODE -ne 0 -or $sourceIntegrity.Trim() -ne "ok") {
    throw "Source database failed SQLite integrity_check: $sourceIntegrity"
}

$stageCreated = $false
try {
    New-Item -ItemType Directory -Path $stageRoot | Out-Null
    $stageCreated = $true
    $assetsDirectory = Join-Path $stageRoot "assets"
    $productsDirectory = Join-Path $assetsDirectory "products"
    $fontsDirectory = Join-Path $stageRoot "fonts"
    $uploadsDirectory = Join-Path $stageRoot "data\assets\products\uploads"
    New-Item -ItemType Directory -Path `
        $assetsDirectory,$productsDirectory,$fontsDirectory,$uploadsDirectory `
        -Force | Out-Null

    Copy-Item -LiteralPath $releaseExecutable `
        -Destination (Join-Path $stageRoot "bakery_pos_gui.exe")
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "assets\crombi-logo.png") `
        -Destination $assetsDirectory
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "assets\toggle_sidebar.png") `
        -Destination $assetsDirectory
    Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "assets\products") `
        -File -Filter "*.jpg" | Copy-Item -Destination $productsDirectory
    Get-ChildItem -LiteralPath (Join-Path $repositoryRoot "assets\fonts") `
        -File | Copy-Item -Destination $fontsDirectory

    $backupCommand = ".backup '$($stagedDatabase.Replace('\','/'))'"
    & $sqliteCli -readonly $SourceDatabase $backupCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Could not create a consistent SQLite snapshot of the source database."
    }
    $snapshotIntegrity =
        (& $sqliteCli -readonly $stagedDatabase "PRAGMA integrity_check;") -join ""
    if ($LASTEXITCODE -ne 0 -or $snapshotIntegrity.Trim() -ne "ok") {
        throw "Packaged database failed SQLite integrity_check: $snapshotIntegrity"
    }
    (Get-Item -LiteralPath $stagedDatabase).IsReadOnly = $false

    foreach ($uploadSource in @($workspaceUploads,$userUploads)) {
        if (-not (Test-Path -LiteralPath $uploadSource -PathType Container)) {
            continue
        }
        foreach ($upload in (Get-ChildItem -LiteralPath $uploadSource -File)) {
            $destination = Join-Path $uploadsDirectory $upload.Name
            if (Test-Path -LiteralPath $destination) {
                $sourceHash = (Get-FileHash -LiteralPath $upload.FullName).Hash
                $destinationHash = (Get-FileHash -LiteralPath $destination).Hash
                if ($sourceHash -ne $destinationHash) {
                    throw "Conflicting uploaded image has the same name: $($upload.Name)"
                }
                continue
            }
            Copy-Item -LiteralPath $upload.FullName -Destination $destination
        }
    }

    $imagePaths = & $sqliteCli -readonly $stagedDatabase `
        "SELECT image_path FROM products WHERE image_path IS NOT NULL AND trim(image_path) <> '';"
    if ($LASTEXITCODE -ne 0) {
        throw "Could not inspect product image paths in the package database."
    }
    foreach ($imagePath in $imagePaths) {
        $slashPath = $imagePath.Replace([char]"\", [char]"/")
        $pathSegments = $slashPath.Split("/")
        $hasParentSegment = [Array]::IndexOf($pathSegments, "..") -ge 0
        if ([System.IO.Path]::IsPathRooted($imagePath) -or $hasParentSegment) {
            throw "Database contains a non-portable product image path: $imagePath"
        }
        $relativePath = $slashPath.Replace(
            [char]"/", [System.IO.Path]::DirectorySeparatorChar)
        $uploadsPrefix = "assets\products\uploads" +
            [System.IO.Path]::DirectorySeparatorChar
        if ($relativePath.StartsWith(
                $uploadsPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $imageRoot = Join-Path $stageRoot "data"
        } else {
            $imageRoot = $stageRoot
        }
        $packagedImage = Join-Path $imageRoot $relativePath
        if (-not (Test-Path -LiteralPath $packagedImage -PathType Leaf)) {
            throw "Referenced product image is missing from package: $imagePath"
        }
    }

    Set-Content -LiteralPath (Join-Path $stageRoot "portable.ini") `
        -Value "[Application]`r`nStorage=portable" -Encoding ASCII
    $readme = @"
Crombi Cookies POS - Portable Windows

Run bakery_pos_gui.exe. This package includes the Qt runtime, SQLite, MinGW
runtime, Windows platform plugin, product photos, and Inter/Poppins fonts.

Extract the folder to a location writable by your Windows account, such as
Documents; avoid protected locations such as Program Files.
Keep the data folder beside the application. The persistent database is
data\bakery_pos.db. Uploaded product images are stored in
data\assets\products\uploads. The app does not replace an existing database;
if the portable database is missing or empty, startup stops with an error.
Back up the complete data folder before replacing or moving the package.

The database in this archive is a consistent snapshot of the selected source
database at package time. Font files are distributed under their included
SIL Open Font Licenses.
"@
    Set-Content -LiteralPath (Join-Path $stageRoot "README.txt") `
        -Value $readme -Encoding UTF8

    $previousPath = $env:PATH
    try {
        $env:PATH = "$qtBin;$mingwRuntime;C:\msys64\ucrt64\bin;$previousPath"
        & $deployTool --release --compiler-runtime --no-translations `
            --dir $stageRoot (Join-Path $stageRoot "bakery_pos_gui.exe")
        if ($LASTEXITCODE -ne 0) {
            throw "windeployqt failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:PATH = $previousPath
    }
    Copy-Item -LiteralPath $sqliteDll -Destination $stageRoot

    $requiredFiles = @(
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Widgets.dll",
        "Qt6PrintSupport.dll",
        "libsqlite3-0.dll",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll",
        "platforms\qwindows.dll",
        "imageformats\qjpeg.dll",
        "fonts\OFL-Inter.txt",
        "fonts\OFL-Poppins.txt",
        "data\bakery_pos.db",
        "data\assets\products\uploads"
    )
    foreach ($relativePath in $requiredFiles) {
        if (-not (Test-Path -LiteralPath (Join-Path $stageRoot $relativePath))) {
            throw "A required package file is missing: $relativePath"
        }
    }

    $outputDirectory = Split-Path -Parent $OutputZip
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [System.IO.Compression.ZipFile]::Open(
        $OutputZip, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in (Get-ChildItem -LiteralPath $stageRoot -File -Recurse)) {
            $relativePath = $file.FullName.Substring($stageRoot.Length).TrimStart([char]"\")
            $entryName = "CrombiCookiesPOS/" + $relativePath.Replace("\", "/")
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive, $file.FullName, $entryName,
                [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    } finally {
        $archive.Dispose()
    }

    $archive = [System.IO.Compression.ZipFile]::OpenRead($OutputZip)
    try {
        $entryNames = @($archive.Entries | ForEach-Object { $_.FullName })
        foreach ($requiredEntry in @(
                "CrombiCookiesPOS/README.txt",
                "CrombiCookiesPOS/bakery_pos_gui.exe",
                "CrombiCookiesPOS/data/bakery_pos.db",
                "CrombiCookiesPOS/platforms/qwindows.dll")) {
            if ($entryNames -notcontains $requiredEntry) {
                throw "The generated ZIP is missing required content: $requiredEntry"
            }
        }
    } finally {
        $archive.Dispose()
    }

    Write-Output "Portable package created: $OutputZip"
    Write-Output "Extracted package folder: $stageRoot"
} catch {
    if (Test-Path -LiteralPath $OutputZip) {
        Remove-Item -LiteralPath $OutputZip -Force
    }
    if ($stageCreated -and (Test-Path -LiteralPath $stageRoot)) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
    throw
}
