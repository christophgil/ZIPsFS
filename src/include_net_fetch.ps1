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
    $path=(($url -replace ':',',') -replace '/',',')
    $path="$DIR\"+($path -replace '.*/','')
    if(!$path){
        Write-Host "Failed. $XRST" -ForegroundColor red
        read-host
    }else{
        ls -l $path
        get-content -head 10  $path  | findstr '.'
        Set-Clipboard "$path"
        "Clibboard:"
        Write-Host $path  -ForegroundColor White -BackgroundColor DarkGreen
    }
    $choice=read-host -Prompt "`nIf you prefere forward slashes, type /. Or press ENTER to start over..."
    if ($choice.trim() -eq '/'){
        $path=$path -replace '\\','/'
        Write-Host $path  -ForegroundColor White -BackgroundColor DarkGreen
        Set-Clipboard "$path"
    }
    read-host "$XRST"
    clear-host
}
