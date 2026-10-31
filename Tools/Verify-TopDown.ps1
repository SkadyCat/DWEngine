param([switch]$Render)
# The old APawn numerical probe was retired with the simplified controller.
& (Join-Path $PSScriptRoot 'Verify-Hero.ps1') -Render:$Render
if(!$?){exit 1}
