#!/usr/bin/env bash
set -u
DIR="${BASH_SOURCE[0]}"
DIR=${DIR%/*}
MD=$DIR/ZIPsFS.1.md
MAN=$DIR/ZIPsFS.1
README_MD=$DIR/README.md

make_man(){
    pandoc $MD -s -t man > $MAN
}
make_readme(){
    cat <<EOF
    # ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

# CURRENT STATE

Usable.

However, we are still fixing minor bugs.

# MOTIVATION

We use closed-source proprietary Windows software and shared libraries for reading experimental data from different types of mass spectrometry machines.
Most data is archived in a read-only WORM file system.

To reduce the number of individual files,  files of one record are bundled in one ZIP.
We hoped that the data inside the ZIP was easily accessible using  pipes or process substitution. Unfortunately, this is not the case  for the proprietary  Windows software we are using.

ZIPsFS has been developed to solve the following  problems:

- Recently  the size of our experiments and thus the number of ZIP files grew enormously.
  Mounting thousands of individual ZIP files at a time is not feasible.

- Some proprietary  software requires write access for files and their parent folders.

- Files may not be read  sequentially. Instead bytes are read  from varying file positions. This is particularly  inefficient
  for compressed  ZIP entries. The worst case is jumping backwards  <I>i.e.</I>  negative seek operation.

- Experimental records are first stored in an intermediate storage and later after verification in the final archive.
  Consequently there are different places to look for a particular file.

- Some proprietary  software fires millions of redundant requests to the file system.
  This is a problem for remote files and mounted ZIP files.

    <DIV style="padding:1em;border:2px solid gray;float:left;">
      File tree with zip files on hard disk:
<BR>
      <PRE style="font-family: monospace,courier,ariel,sans-serif;">
├── <B style="color:#1111FF;">src</B>
│   ├── <B style="color:#1111FF;">InstallablePrograms</B>
│   │   └── some_software.zip
│   │   └── my_manuscript.zip
└── <B style="color:#1111FF;">read-write</B>
   ├── my_manuscript.zip.Content
           ├── my-modified-text.txt
      </PRE>
    </div>

    <DIV style="padding:1em;border:2px solid gray;float:right;">
      Virtual file tree presented by ZIPsFS:
      <PRE style="font-family: monospace,courier,ariel,sans-serif;">
├── <B style="color:#1111FF;">InstallablePrograms</B>
│   ├── some_software.zip
│   └── <B style="color:#1111FF;">some_software.zip.Content</B>
│       ├── help.html
│       ├── program.dll
│       └── program.exe
│   ├── my_manuscript.zip
│   └── <B style="color:#1111FF;">my_manuscript.zip.Content</B>
│       ├── my_text.tex
│       ├── my_lit.bib
│       ├── fig1.png
│       └── fig2.png
      </PRE>
    </DIV>
    <DIV style="clear:both;">
    The file tree can be adapted to specific needs by editing <I>ZIPsFS_configuration.c</I>.
    Our mass-spectrometry files are processed with special software.
    It expects a file tree in its original form i.e. as files would not have been zipped.
    Furthermore, write permission is required for files and containing folders while files are permanently stored and cannot be modified any more.
    The folder names need to be ".d" instead of ".d.Zip.Content".
    For Sciex (zenotof) machines, all files must be in one folder without intermediate folders.
    </DIV>
     <DIV style="padding:1em;border:2px solid gray;float:left;">
                    File tree with zip files on hard disk:
      <PRE style="font-family: monospace,courier,ariel,sans-serif;">
├── <B style="color:#1111FF;">brukertimstof</B>
│   └── <B style="color:#1111FF;">202302</B>
│       ├── 20230209_hsapiens_Sample_001.d.Zip
│       ├── 20230209_hsapiens_Sample_002.d.Zip
│       └── 20230209_hsapiens_Sample_003.d.Zip

...

│       └── 20230209_hsapiens_Sample_099.d.Zip
└── <B style="color:#1111FF;">zenotof</B>
   └── <B style="color:#1111FF;">202304</B>
   ├── 20230402_hsapiens_Sample_001.wiff2.Zip
   ├── 20230402_hsapiens_Sample_002.wiff2.Zip
   └── 270230402_hsapiens_Sample_003.wiff2.Zip
...
        └── 270230402_hsapiens_Sample_099.wiff2.Zip
      </PRE>
    </DIV>


    <DIV style="padding:1em;border:2px solid gray;float:right;">
            Virtual file tree presented by ZIPsFS:
            <PRE style="font-family: monospace,courier,ariel,sans-serif;">
├── <B style="color:#1111FF;">brukertimstof</B>
│   └── <B style="color:#1111FF;">202302</B>
│       ├── <B style="color:#1111FF;">20230209_hsapiens_Sample_001.d</B>
│       │   ├── analysis.tdf
│       │   └── analysis.tdf_bin
│       ├── <B style="color:#1111FF;">20230209_hsapiens_Sample_002.d</B>
│       │   ├── analysis.tdf
│       │   └── analysis.tdf_bin
│       └── <B style="color:#1111FF;">20230209_hsapiens_Sample_003.d</B>
│           ├── analysis.tdf
│           └── analysis.tdf_bin

...

│       └── <B style="color:#1111FF;">20230209_hsapiens_Sample_099.d</B>
│           ├── analysis.tdf
│           └── analysis.tdf_bin
└── <B style="color:#1111FF;">zenotof</B>
    └── <B style="color:#1111FF;">202304</B>
          ├── 20230402_hsapiens_Sample_001.timeseries.data
          ├── 20230402_hsapiens_Sample_001.wiff
          ├── 20230402_hsapiens_Sample_001.wiff2
          ├── 20230402_hsapiens_Sample_001.wiff.scan
          ├── 20230402_hsapiens_Sample_002.timeseries.data
          ├── 20230402_hsapiens_Sample_002.wiff
          ├── 20230402_hsapiens_Sample_002.wiff2
          ├── 20230402_hsapiens_Sample_002.wiff.scan
          ├── 20230402_hsapiens_Sample_003.timeseries.data
          ├── 20230402_hsapiens_Sample_003.wiff
          ├── 20230402_hsapiens_Sample_003.wiff2
          └── 20230402_hsapiens_Sample_003.wiff.scan

...

          ├── 20230402_hsapiens_Sample_099.timeseries.data
          ├── 20230402_hsapiens_Sample_099.wiff
          ├── 20230402_hsapiens_Sample_099.wiff2
          └── 20230402_hsapiens_Sample_099.wiff.scan
          </PRE>
    </DIV>
        <DIV style="clear:both;"></DIV>



EOF
    echo
    cat $MD
}


DOCU_TREE=~/tmp/ZIPsFS/doc/tree
docu_f1(){
    local d0=$DOCU_TREE/mnt  f
    [[ $1 == s ]]&&d0=$DOCU_TREE/src
    shift
    for f in $*;do
        f=$d0/$f
        [[ -e $f ]] && continue
        mkdir -p ${f%/*} && touch $f
    done
}
## tree -C -t  -H '' --nolinks ~/tmp/ZIPsFS/doc/tree/ | sed -n '/<body/,/directories/p' | grep -v -e 'directories' -e '<body>'
docu_create_file_tree(){
    local z=InstallablePrograms/some_software.zip
    docu_f1 s $z
    docu_f1 m ${z}{,.Content/program.exe,.Content/program.dll,.Content/help.html}
    local b=zenotof/202304/20230402_hsapiens_serum_Sample_00
    docu_f1 s ${b}{1,2,3}.wiff2.Zip
    docu_f1 m ${b}{1,2,3}.{wiff,wiff2,wiff.scan,timeseries.data}
    local b=brukertimstof/202302/20230209_hsapiens_serum_Sample_00
    docu_f1 s ${b}{1,2,3}.d.Zip
    docu_f1 m ${b}{1,2,3}.d/analysis.tdf{,_bin}
}

main1(){
    docu_create_file_tree
    tree $DOCU_TREE/
}

main(){
    local msg="${1:-}"
    if [[ -z $msg ]]; then
        echo "Missing: commit message"
    else

        make_man
        make_readme > $README_MD

        git commit -a -m "$msg"
        git push origin
    fi
}


main "$@"
