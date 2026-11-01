param([Parameter(Mandatory)][ValidateSet('resume','tour','overview','mode','pose','focus','shot','audit','equipmentaudit','equipmentbuild','equipmentrefresh','lighting','sensors')][string]$Action,[string]$Model='',[string]$Clip='idle',[double]$Phase=0.75,[double]$Angle=0.4,[ValidateSet('opus','controlled')][string]$Mode='opus',[string]$Name='capture')
$ErrorActionPreference='Stop'
$c3Root=Split-Path $PSScriptRoot -Parent
$c3Command=switch($Action){
 'pose'{"pose $Model $Clip $Phase $Angle"}
 'focus'{"focus $Model"}
 'mode'{"mode $Mode"}
 'shot'{"shot $Name"}
 default{$Action}
}
$c3Request=[guid]::NewGuid().ToString('N')+' '+$c3Command
[System.IO.File]::WriteAllText((Join-Path $c3Root 'Saved\control.txt'),$c3Request,[System.Text.UTF8Encoding]::new($false))
Write-Host $c3Command
