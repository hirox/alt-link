http://pocoproject.org/

PoCo 1.6.1 ���_�E�����[�h
Visual Studio 2015 �� static_mt �Ńr���h

�r���h�菇
==
\poco-1.6.1>buildwin.cmd
Usage:
------
buildwin VS_VERSION [ACTION] [LINKMODE] [CONFIGURATION] [PLATFORM] [SAMPLES] [TESTS] [TOOL]
VS_VERSION:    "90|100|110|120|140"
ACTION:        "build|rebuild|clean"
LINKMODE:      "static_mt|static_md|shared|all"
CONFIGURATION: "release|debug|both"
PLATFORM:      "Win32|x64|WinCE|WEC2013"
SAMPLES:       "samples|nosamples"
TESTS:         "tests|notests"
TOOL:          "devenv|vcexpress|wdexpress|msbuild"

Default is build all.

\poco-1.6.1>buildwin.cmd 140 build static_mt both Win32 nosamples notests
