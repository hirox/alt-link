## clone元
https://github.com/signal11/hidapi.git
d17db57b9d4354752e0af42f5f33007a42ef2906

## ライセンス
選べるらしい。BSDを選べばスタティックリンクするのも問題なさそう
- ./LICENSE.txt

## Windows
- lib/hidapi.lib
- lib/hidapid.lib
Visual Studio 2013 でBuild
- VS2013 vcxprojへコンバート
- ラインタイムライブラリを/MT、/MTdに変更し、DLL/LIBで競合しないようにした
- 成果物を、DLL->ライブラリに変更し、LIBを生成

## Mac OS X
- lib/libhidapi.la
Mac OS X Yosemite 64bitでBuild
