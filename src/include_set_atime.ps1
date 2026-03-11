$SFX_ATIME=".magicSfxSetAccessTime"  #FILTER_OUT
function ask_not_existing(){ echo 'ASK_NOT_EXISTING';}       #FILTER_OUT
function ask_how_long_ago(){ echo 'HOW LONG AGO';}      #FILTER_OUT
function how_select_files(){ echo 'HOW COPY PATHS';}      #FILTER_OUT
$ASK_NOT_EXISTING='ASK_NOT_EXISTING'                     #FILTER_OUT



ask_how_long_ago
$h=Read-Host -Prompt 'How many hours ago? '
$h=($h).Trim()
if ($h -match 'h$'){$mult=1}
if ($h -match 'd$'){$mult=24}
if ($h -match 'w$'){$mult=24*7}
if ($h -match 'm$'){$mult=24*30}
if ($h -match 'y$'){$mult=24*365}


if ($mult -ne 0){ $h=$mult*[int]( $h -replace '.$');}
echo "Hours: $h"

how_select_files
$ff=$args
if (!$ff -or !$ff.Length){ $ff=$(Get-Clipboard -format filedroplist);}
$no_comput=1; $h=0



if ($ff){
		ask_not_existing
		$no_comput=Read-Host -Prompt "$ASK_NOT_EXISTING"
}else{
		echo "$NO_FF"
}

foreach($f in $ff){
		if ($f){
		Write-Host -ForegroundColor green -BackgroundColor white "Processing $f ..."
		if (!$no_comput){
				$r=New-Object System.IO.StreamReader -Arg (Resolve-Path "$f")
				$r.ReadLine() |Format-Hex
				$r.close()
				gc "$f.log" 2>$NULL
		}
				ls $(-join($f,"$SFX_ATIME",(0+$($h -replace '[^0-9-+]')))) 2>$NULL;
		}
}
read-host -Prompt 'Press Enter'
