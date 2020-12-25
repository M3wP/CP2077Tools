# CP2077Tools
Tools for CP2077


Just quickly until I can put something decent here.

I have fixed a few issues in the Pascal bnkextr tool.  This tool (any source version) isn't working correctly at this time.

I have made a tool to dump meta data from WAV/WEM files called wavdatdmp

	Usage:  wavdatdmp <infile>

I have made a tool to append a RIFF/WAVEfmt header to a raw file called raw2wav

	Usage: raw2wav <infile>

I have made a tool to extract and decode the WAV files in an OPUSPAK file.  You need opuspakdec.exe, libcrypto-3.dll and libssl-3.dll.  I haven't included the source for this yet as I need to figure out how to package it given changes to core libraries.

	Usage: opuspakdec <infile>


Enjoy.

Daniel.
