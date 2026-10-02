param(
    [string]$SourceDir = (Join-Path $PSScriptRoot '..\wayback\~alan\MS-html'),
    [string]$OutputDir = $PSScriptRoot
)

$ErrorActionPreference = 'Stop'

$classes = @{
    'fig7.GIF'  = @{ Category = 'actual application screenshot'; Note = 'Figure 7: Spectrogram user interface.' }
    'fig1.gif'  = @{ Category = 'signal visualization'; Note = 'Figure 1: signal, analysis window, and windowed signal.' }
    'fig2.gif'  = @{ Category = 'signal visualization'; Note = 'Figure 2: overlapping analysis windows.' }
    'fig3.gif'  = @{ Category = 'signal visualization'; Note = 'Figure 3: overlap-add reconstruction.' }
    'snap.gif'  = @{ Category = 'signal visualization'; Note = 'Figure 5: spectral surface from a short trumpet note.' }
    'fof.gif'   = @{ Category = 'signal visualization'; Note = 'Figure 8: multi-FOF filter frequency response.' }
    'f51.GIF'   = @{ Category = 'block diagram'; Note = 'Figure 4: source-filter signal model.' }
    'fig61.GIF' = @{ Category = 'block diagram'; Note = 'Figure 6: Spectrogram software environment.' }
}

$colors = @{
    'actual application screenshot' = '#8b2bd6'
    'signal visualization'           = '#007a78'
    'block diagram'                  = '#b25b00'
    'mathematical figure'            = '#455a64'
    'photograph'                     = '#2f6db0'
    'irrelevant'                     = '#777777'
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$cardDir = Join-Path $OutputDir '_cards'
New-Item -ItemType Directory -Force -Path $cardDir | Out-Null

$rows = foreach ($file in Get-ChildItem -LiteralPath $SourceDir -Filter '*.gif' | Sort-Object Name) {
    $entry = $classes[$file.Name]
    if ($null -eq $entry) {
        $category = 'mathematical figure'
        $note = 'Equation or mathematical-symbol image embedded in the HTML thesis export.'
    } else {
        $category = $entry.Category
        $note = $entry.Note
    }

    [pscustomobject]@{
        File = $file.Name
        Category = $category
        Note = $note
        Source = $file.FullName
    }
}

if ($rows.Count -ne 65) {
    throw "Expected 65 GIFs, found $($rows.Count)."
}

$manifestPath = Join-Path $OutputDir 'classification.csv'
$rows | Select-Object File, Category, Note | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding utf8

$cards = foreach ($row in $rows) {
    $safeName = [IO.Path]::GetFileNameWithoutExtension($row.File) -replace '[^A-Za-z0-9._-]', '_'
    $cardPath = Join-Path $cardDir ($safeName + '.png')
    $label = $row.File + "`n" + $row.Category
    $color = $colors[$row.Category]
    & magick $row.Source -coalesce -thumbnail '292x174>' -background white -gravity center -extent 300x182 `
        -gravity south -splice '0x54' -fill $color -draw 'rectangle 0,182 299,235' `
        -fill white -font Arial -pointsize 13 -interline-spacing 2 -annotate '+0+9' $label $cardPath
    if ($LASTEXITCODE -ne 0) { throw "ImageMagick failed for $($row.File)." }
    $cardPath
}

$bodyPath = Join-Path $OutputDir 'peevers-65-contact-sheet-classified-body.png'
& magick montage @cards -background '#e8ecef' -tile '5x13' -geometry '300x236+8+8' $bodyPath
if ($LASTEXITCODE -ne 0) { throw 'ImageMagick montage failed.' }

$headerPath = Join-Path $OutputDir '_classified-header.png'
& magick -size '1580x190' canvas:'#182126' -fill white -font Arial -pointsize 34 -gravity north-west `
    -annotate '+34+27' 'Peevers Berkeley UI, 1993–94 — contextual reference only' `
    -pointsize 20 -fill '#d8e0e4' -annotate '+34+82' '65 recovered GIF assets | source images preserved unchanged' `
    -pointsize 17 -fill '#c4cdd2' -annotate '+34+124' '1 application screenshot | 5 signal visualizations | 2 block diagrams | 57 mathematical figures | 0 photographs | 0 irrelevant' `
    $headerPath
if ($LASTEXITCODE -ne 0) { throw 'ImageMagick header generation failed.' }

$sheetPath = Join-Path $OutputDir 'peevers-65-contact-sheet-classified.png'
& magick $headerPath $bodyPath -append $sheetPath
if ($LASTEXITCODE -ne 0) { throw 'ImageMagick final append failed.' }

Write-Output $sheetPath
Write-Output $manifestPath
