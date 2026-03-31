Set-PsDebug -strict
$DIR=$PSScriptRoot
. $PSScriptRoot\include_net_fetch_common.ps1 #FILTER_OUT
$DIR='\\s-mcpb-ms03\union\test\is\ZIPsFS\n'  #FILTER_OUT
$cc=@($c0,$c1,$c2,$c3)
For(;;){
		$url=''
		do{
				"`n"
				$cc|%{$i=0}{" ($i)  $_";$i++}
				$choice=(Read-Host -Prompt "$XCHOICE").trim()
				if ($choice -eq ''){ exit 0}
				if ($choice -eq '0'){Start-process $URL_UP;continue}
				if ($cc[$choice]){ $choice=$cc[$choice];}
				if ($choice -match '://'){
						$url=$choice
				}elseif ($choice -match '.fasta(\.gz)? '){
						$url=$FTP_EXPASY+($choice -replace '(\.fasta)(\.gz)? .*','$1$2')
				}
		}until($url)

		if ($url -match '\.gz$' -and (Read-Host "`n$XGZ").ToLower() -notmatch 'y'){ $url=$url -replace '.{3}$'}

		"`nurl=$url`n"
		$html=$dir+'/_UPDATE_/'+$url.replace('/',',').replace(':',',')+'.html'
		XHTML
		$table=$(gc $html|?{$_ -match "`t"}|%{($_ -replace '<[^>]+>','').Trim()}|ConvertFrom-Csv -Delimiter "`t")
		$table|ft|Out-String -Stream -Width 999
		$path=''
		$table|%{if($_.Status -match 'elative'){$path=$_.Path}}
		$Path="$DIR\"+($Path -replace '.*/','')
		if(!$path){
				Write-Host "Failed. $XRST" -ForegroundColor red
				read-host
		}else{
				ls -l $Path
				Set-Clipboard "$Path"
				"Clibboard:"
				Write-Host $Path  -ForegroundColor White -BackgroundColor DarkGreen
				Write-Host $XCV
		}
		$choice=read-host -Prompt "`nIf you prefere forward slashes, type /. Or press ENTER to start over..."
		if ($choice.trim() -eq '/'){
				$path=$path -replace '\\','/'
				Write-Host $Path  -ForegroundColor White -BackgroundColor DarkGreen
				Set-Clipboard "$Path"
				Write-Host $XCV
		}
		read-host "$XRST"
		cls
}
