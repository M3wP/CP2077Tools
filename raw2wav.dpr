program raw2wav;

{$APPTYPE CONSOLE}

{$R *.res}

uses
	System.SysUtils, System.Classes;

type
	TRIFFHdr = packed record
		id: array[0..3] of AnsiChar;
		size:  Integer;
		wav: array[0..3] of AnsiChar;
	end;

	TBlockHdr = packed record
		id: array[0..3] of AnsiChar;
		size:  Integer;
	end;

	TWVFMTBlock = packed record
		afmt: Word;     		//Audio Format
		achn: Word;				//Number of channels
		rate: Cardinal;			//Sample rate
		bytr: Cardinal;			//Bytes rate
		baln: Word;				//Block align
		bpsm: Word;				//Bits per sample
	end;

var
	fi,
	fo: TFileStream;
	rh: TRIFFHdr;
	wh: TBlockHdr;
	wf: TWVFMTBlock;
	dh: TBlockHdr;

	sr: Cardinal;
	ch: Word;
	bp: Word;
	ba: Word;

	i: Integer;
	ni,
	no: string;

begin
try
//	Defaults.  Make switches to change.
	sr:= 48000;	//48kHz
	ch:= 1;		//mono
	bp:= 16;	//16 bit
	ba:= 0;		//signed

	rh.id:= AnsiString('RIFF');
	rh.wav:= AnsiString('WAVE');

	wh.id:= AnsiString('fmt ');
	wh.size:= SizeOf(TWVFMTBlock);

	wf.afmt:= 1;		//PCM
	wf.achn:= ch;
	wf.rate:= sr;
	wf.bytr:= sr * ch * bp div 8;
	wf.baln:= ch * bp div 8;
	wf.bpsm:= bp;

	dh.id:= AnsiString('data');

	ni:= ParamStr(1);
	no:= ChangeFileExt(ni, '.wav');

	fi:= TFileStream.Create(ni, fmOpenRead);
	try
		dh.size:= fi.Size;
		rh.size:= dh.size + 2 * SizeOf(TBlockHdr) + SizeOf(TWVFMTBlock);

		fo:= TFileStream.Create(no, fmCreate);
		try
			fo.Write(rh, SizeOf(TRIFFHdr));
			fo.Write(wh, SizeOf(TBlockHdr));
			fo.Write(wf, SizeOf(TWVFMTBlock));
			fo.Write(dh, SizeOf(TBlockHdr));

			fo.CopyFrom(fi, fi.Size);

			finally
			fo.Free;
			end;

		finally
		fi.Free;
		end;

	except
	on E: Exception do
		Writeln(E.ClassName, ': ', E.Message);
	end;
end.
