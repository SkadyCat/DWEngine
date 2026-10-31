$ErrorActionPreference='Stop'
$opusRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$opusSolution=Join-Path $opusRoot 'DungeonWorldOpus.sln'
$opusText=[System.IO.File]::ReadAllText($opusSolution)
$opusEngineProject=[regex]::Match($opusText,'(?m)^Project\("\{[^}]+\}"\) = "UE5", "[^"]+", "(?<id>\{[^}]+\})"\r?$')
if(!$opusEngineProject.Success){throw 'Could not locate the UE5 project in the Opus solution.'}
# Retain all engine browsing/configuration entries; exclude engine build nodes.
$opusEngineBuild='(?m)^[\t ]*'+[regex]::Escape($opusEngineProject.Groups['id'].Value)+'\.[^\r\n]*\.Build\.0[^\r\n]*\r?\n'
$opusConfigured=[regex]::Replace($opusText,$opusEngineBuild,'')
if($opusConfigured-ne $opusText){[System.IO.File]::WriteAllText($opusSolution,$opusConfigured,[System.Text.UTF8Encoding]::new($false))}
Write-Host 'Opus solution builds project modules against the precompiled engine; UE5 source remains visible.'
