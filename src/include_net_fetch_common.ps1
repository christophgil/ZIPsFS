echo '
*******************************************************************
*** This script helps downloading files from the Internet.      ***
*** You will be asked for the URL.                              ***
*** The remote file will be downloaded/updated and the          ***
*** canonicalized local file path will be copied to Clipboard   ***
*******************************************************************

Note:
	 The version from the release notes file may be included into the local path.
	 The local path may become canonicalized.
	 All this can be configured in ZIPsFS_configuration_internet.c'


$c0='UniProt in Web browser    (R-click browser link "Download ...FASTA and chose "Copy Link Target")'
$c1='Eukaryota/UP000005640/UP000005640_9606.fasta (Human) - example'
$c2='Eukaryota/UP000000589/UP000000589_10090.fasta.gz (Mouse) - example with gz'
$c3='Eukaryota/UP000501346/UP000501346_27292.fasta (Saccharomyces cerevisiae x) - example'

$URL_UP='https://www.uniprot.org/proteomes/UP000005640'
$FTP_EXPASY='ftp://ftp.expasy.org/databases/uniprot/current_release/knowledgebase/reference_proteomes/'
$XRST='Press ENTER to start over'
$XCV='The local file path can be pasted with Ctrl-V.'
$XGZ='With gz compression [y/N]?'
$XCHOICE='Choose options 0...4 or enter  URL '

function XHTML(){
		echo "The file $html provides the local path - Please Wait ..."
}
