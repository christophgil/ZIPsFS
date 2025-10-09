//////////////////////////////////////////////////////////////////////////////////////
// https://www.utf8-chartable.de/unicode-utf8-table.pl?start=9472&unicodeinhtml=dec //
//////////////////////////////////////////////////////////////////////////////////////
#ifndef _cg_unicode_dot_c
#define _cg_unicode_dot_c
#include <stdbool.h>
#include <stdio.h>



#define BD_LIGHT_HORIZONTAL                          0x25009480
#define BD_HEAVY_HORIZONTAL                          0x25019481
#define BD_LIGHT_VERTICAL                            0x25029482
#define BD_HEAVY_VERTICAL                            0x25039483
#define BD_LIGHT_TRIPLE_DASH_HORIZONTAL              0x25049484
#define BD_HEAVY_TRIPLE_DASH_HORIZONTAL              0x25059485
#define BD_LIGHT_TRIPLE_DASH_VERTICAL                0x25069486
#define BD_HEAVY_TRIPLE_DASH_VERTICAL                0x25079487
#define BD_LIGHT_QUADRUPLE_DASH_HORIZONTAL           0x25089488
#define BD_HEAVY_QUADRUPLE_DASH_HORIZONTAL           0x25099489
#define BD_LIGHT_QUADRUPLE_DASH_VERTICAL             0x250A948a
#define BD_HEAVY_QUADRUPLE_DASH_VERTICAL             0x250B948b
#define BD_LIGHT_DOWN_AND_RIGHT                      0x250C948c
#define BD_DOWN_LIGHT_AND_RIGHT_HEAVY                0x250D948d
#define BD_DOWN_HEAVY_AND_RIGHT_LIGHT                0x250E948e
#define BD_HEAVY_DOWN_AND_RIGHT                      0x250F948f
#define BD_LIGHT_DOWN_AND_LEFT                       0x25109490
#define BD_DOWN_LIGHT_AND_LEFT_HEAVY                 0x25119491
#define BD_DOWN_HEAVY_AND_LEFT_LIGHT                 0x25129492
#define BD_HEAVY_DOWN_AND_LEFT                       0x25139493
#define BD_LIGHT_UP_AND_RIGHT                        0x25149494
#define BD_UP_LIGHT_AND_RIGHT_HEAVY                  0x25159495
#define BD_UP_HEAVY_AND_RIGHT_LIGHT                  0x25169496
#define BD_HEAVY_UP_AND_RIGHT                        0x25179497
#define BD_LIGHT_UP_AND_LEFT                         0x25189498
#define BD_UP_LIGHT_AND_LEFT_HEAVY                   0x25199499
#define BD_UP_HEAVY_AND_LEFT_LIGHT                   0x251A949a
#define BD_HEAVY_UP_AND_LEFT                         0x251B949b
#define BD_LIGHT_VERTICAL_AND_RIGHT                  0x251C949c
#define BD_VERTICAL_LIGHT_AND_RIGHT_HEAVY            0x251D949d
#define BD_UP_HEAVY_AND_RIGHT_DOWN_LIGHT             0x251E949e
#define BD_DOWN_HEAVY_AND_RIGHT_UP_LIGHT             0x251F949f
#define BD_VERTICAL_HEAVY_AND_RIGHT_LIGHT            0x252094a0
#define BD_DOWN_LIGHT_AND_RIGHT_UP_HEAVY             0x252194a1
#define BD_UP_LIGHT_AND_RIGHT_DOWN_HEAVY             0x252294a2
#define BD_HEAVY_VERTICAL_AND_RIGHT                  0x252394a3
#define BD_LIGHT_VERTICAL_AND_LEFT                   0x252494a4
#define BD_VERTICAL_LIGHT_AND_LEFT_HEAVY             0x252594a5
#define BD_UP_HEAVY_AND_LEFT_DOWN_LIGHT              0x252694a6
#define BD_DOWN_HEAVY_AND_LEFT_UP_LIGHT              0x252794a7
#define BD_VERTICAL_HEAVY_AND_LEFT_LIGHT             0x252894a8
#define BD_DOWN_LIGHT_AND_LEFT_UP_HEAVY              0x252994a9
#define BD_UP_LIGHT_AND_LEFT_DOWN_HEAVY              0x252A94aa
#define BD_HEAVY_VERTICAL_AND_LEFT                   0x252B94ab
#define BD_LIGHT_DOWN_AND_HORIZONTAL                 0x252C94ac
#define BD_LEFT_HEAVY_AND_RIGHT_DOWN_LIGHT           0x252D94ad
#define BD_RIGHT_HEAVY_AND_LEFT_DOWN_LIGHT           0x252E94ae
#define BD_DOWN_LIGHT_AND_HORIZONTAL_HEAVY           0x252F94af
#define BD_DOWN_HEAVY_AND_HORIZONTAL_LIGHT           0x253094b0
#define BD_RIGHT_LIGHT_AND_LEFT_DOWN_HEAVY           0x253194b1
#define BD_LEFT_LIGHT_AND_RIGHT_DOWN_HEAVY           0x253294b2
#define BD_HEAVY_DOWN_AND_HORIZONTAL                 0x253394b3
#define BD_LIGHT_UP_AND_HORIZONTAL                   0x253494b4
#define BD_LEFT_HEAVY_AND_RIGHT_UP_LIGHT             0x253594b5
#define BD_RIGHT_HEAVY_AND_LEFT_UP_LIGHT             0x253694b6
#define BD_UP_LIGHT_AND_HORIZONTAL_HEAVY             0x253794b7
#define BD_UP_HEAVY_AND_HORIZONTAL_LIGHT             0x253894b8
#define BD_RIGHT_LIGHT_AND_LEFT_UP_HEAVY             0x253994b9
#define BD_LEFT_LIGHT_AND_RIGHT_UP_HEAVY             0x253A94ba
#define BD_HEAVY_UP_AND_HORIZONTAL                   0x253B94bb
#define BD_LIGHT_VERTICAL_AND_HORIZONTAL             0x253C94bc
#define BD_LEFT_HEAVY_AND_RIGHT_VERTICAL_LIGHT       0x253D94bd
#define BD_RIGHT_HEAVY_AND_LEFT_VERTICAL_LIGHT       0x253E94be
#define BD_VERTICAL_LIGHT_AND_HORIZONTAL_HEAVY       0x253F94bf
#define BD_UP_HEAVY_AND_DOWN_HORIZONTAL_LIGHT        0x25409580
#define BD_DOWN_HEAVY_AND_UP_HORIZONTAL_LIGHT        0x25419581
#define BD_VERTICAL_HEAVY_AND_HORIZONTAL_LIGHT       0x25429582
#define BD_LEFT_UP_HEAVY_AND_RIGHT_DOWN_LIGHT        0x25439583
#define BD_RIGHT_UP_HEAVY_AND_LEFT_DOWN_LIGHT        0x25449584
#define BD_LEFT_DOWN_HEAVY_AND_RIGHT_UP_LIGHT        0x25459585
#define BD_RIGHT_DOWN_HEAVY_AND_LEFT_UP_LIGHT        0x25469586
#define BD_DOWN_LIGHT_AND_UP_HORIZONTAL_HEAVY        0x25479587
#define BD_UP_LIGHT_AND_DOWN_HORIZONTAL_HEAVY        0x25489588
#define BD_RIGHT_LIGHT_AND_LEFT_VERTICAL_HEAVY       0x25499589
#define BD_LEFT_LIGHT_AND_RIGHT_VERTICAL_HEAVY       0x254A958a
#define BD_HEAVY_VERTICAL_AND_HORIZONTAL             0x254B958b
#define BD_LIGHT_DOUBLE_DASH_HORIZONTAL              0x254C958c
#define BD_HEAVY_DOUBLE_DASH_HORIZONTAL              0x254D958d
#define BD_LIGHT_DOUBLE_DASH_VERTICAL                0x254E958e
#define BD_HEAVY_DOUBLE_DASH_VERTICAL                0x254F958f
#define BD_DOUBLE_HORIZONTAL                         0x25509590
#define BD_DOUBLE_VERTICAL                           0x25519591
#define BD_DOWN_SINGLE_AND_RIGHT_DOUBLE              0x25529592
#define BD_DOWN_DOUBLE_AND_RIGHT_SINGLE              0x25539593
#define BD_DOUBLE_DOWN_AND_RIGHT                     0x25549594
#define BD_DOWN_SINGLE_AND_LEFT_DOUBLE               0x25559595
#define BD_DOWN_DOUBLE_AND_LEFT_SINGLE               0x25569596
#define BD_DOUBLE_DOWN_AND_LEFT                      0x25579597
#define BD_UP_SINGLE_AND_RIGHT_DOUBLE                0x25589598
#define BD_UP_DOUBLE_AND_RIGHT_SINGLE                0x25599599
#define BD_DOUBLE_UP_AND_RIGHT                       0x255A959a
#define BD_UP_SINGLE_AND_LEFT_DOUBLE                 0x255B959b
#define BD_UP_DOUBLE_AND_LEFT_SINGLE                 0x255C959c
#define BD_DOUBLE_UP_AND_LEFT                        0x255D959d
#define BD_VERTICAL_SINGLE_AND_RIGHT_DOUBLE          0x255E959e
#define BD_VERTICAL_DOUBLE_AND_RIGHT_SINGLE          0x255F959f
#define BD_DOUBLE_VERTICAL_AND_RIGHT                 0x256095a0
#define BD_VERTICAL_SINGLE_AND_LEFT_DOUBLE           0x256195a1
#define BD_VERTICAL_DOUBLE_AND_LEFT_SINGLE           0x256295a2
#define BD_DOUBLE_VERTICAL_AND_LEFT                  0x256395a3
#define BD_DOWN_SINGLE_AND_HORIZONTAL_DOUBLE         0x256495a4
#define BD_DOWN_DOUBLE_AND_HORIZONTAL_SINGLE         0x256595a5
#define BD_DOUBLE_DOWN_AND_HORIZONTAL                0x256695a6
#define BD_UP_SINGLE_AND_HORIZONTAL_DOUBLE           0x256795a7
#define BD_UP_DOUBLE_AND_HORIZONTAL_SINGLE           0x256895a8
#define BD_DOUBLE_UP_AND_HORIZONTAL                  0x256995a9
#define BD_VERTICAL_SINGLE_AND_HORIZONTAL_DOUBLE     0x256A95aa
#define BD_VERTICAL_DOUBLE_AND_HORIZONTAL_SINGLE     0x256B95ab
#define BD_DOUBLE_VERTICAL_AND_HORIZONTAL            0x256C95ac
#define BD_LIGHT_ARC_DOWN_AND_RIGHT                  0x256D95ad
#define BD_LIGHT_ARC_DOWN_AND_LEFT                   0x256E95ae
#define BD_LIGHT_ARC_UP_AND_LEFT                     0x256F95af
#define BD_LIGHT_ARC_UP_AND_RIGHT                    0x257095b0
#define BD_LIGHT_DIAGONAL_UPPER_RIGHT_TO_LOWER_LEFT  0x257195b1
#define BD_LIGHT_DIAGONAL_UPPER_LEFT_TO_LOWER_RIGHT  0x257295b2
#define BD_LIGHT_DIAGONAL_CROSS                      0x257395b3
#define BD_LIGHT_LEFT                                0x257495b4
#define BD_LIGHT_UP                                  0x257595b5
#define BD_LIGHT_RIGHT                               0x257695b6
#define BD_LIGHT_DOWN                                0x257795b7
#define BD_HEAVY_LEFT                                0x257895b8
#define BD_HEAVY_UP                                  0x257995b9
#define BD_HEAVY_RIGHT                               0x257A95ba
#define BD_HEAVY_DOWN                                0x257B95bb
#define BD_LIGHT_LEFT_AND_HEAVY_RIGHT                0x257C95bc
#define BD_LIGHT_UP_AND_HEAVY_DOWN                   0x257D95bd
#define BD_HEAVY_LEFT_AND_LIGHT_RIGHT                0x257E95be
#define BD_HEAVY_UP_AND_LIGHT_DOWN                   0x257F95bf


/*======================================================\
| Creates UTF8 for UNIX console or HTML-unicode string. |
\======================================================*/
static int cg_unicode(char *s, const int c,const bool html){
  if (html){
    return sprintf(s,"&#%d;",c>>16);
  }else{
    s[0]=0xe2; s[1]=(c>>8)&255; s[2]=c&255; s[3]=0;
    return 3;
  }
}
#endif //_cg_unicode_dot_c
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0

#include <stdlib.h>

int main(int argc, char *argv[]){
  char unicode[9];
#define P() fputs(unicode,stderr)
#define U(code) cg_unicode(unicode, code,false)
#define T(times) for(int iTimes=times;--iTimes>=0;) P()
#define N() fputs("\n",stdout)
  U(BD_HEAVY_DOWN_AND_RIGHT);P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_DOWN_AND_HORIZONTAL);P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_DOWN_AND_LEFT);P();
  N();
  //
  U(BD_HEAVY_VERTICAL_AND_RIGHT);P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_VERTICAL_AND_HORIZONTAL); P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_VERTICAL_AND_LEFT); P();
  N();
  //
  U(BD_VERTICAL_HEAVY_AND_RIGHT_LIGHT);P();
  U(BD_LIGHT_HORIZONTAL);  T(4);
  U(BD_VERTICAL_HEAVY_AND_HORIZONTAL_LIGHT); P();
  U(BD_LIGHT_HORIZONTAL);  T(4);
  U(BD_VERTICAL_HEAVY_AND_LEFT_LIGHT); P();
  N();
  //
  U(BD_HEAVY_UP_AND_RIGHT);P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_UP_AND_HORIZONTAL);P();
  U(BD_HEAVY_HORIZONTAL);  T(4);
  U(BD_HEAVY_UP_AND_LEFT);P();
  N();

  cg_unicode(unicode,BD_LIGHT_VERTICAL_AND_RIGHT,true);
  fprintf(stderr,"BD_LIGHT_VERTICAL_AND_RIGHT should be &#9500;  is %s\n",unicode);

}
#endif
