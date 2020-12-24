program wavdatdmp;

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

	TWAVEfmt = packed record
		id:  array[0..3] of AnsiChar;
		size: Integer;
	end;

	TBlockHdr = packed record
		id: array[0..3] of AnsiChar;
		size:  Integer;
	end;

var
	f: TFileStream;
	d: TMemoryStream;
	o: TFileStream;
	r: TRIFFHdr;
	w: TWAVEfmt;
	b: TBlockHdr;


procedure DumpHexData(AStream: TStream);
	var
	i: Integer;
	s: string;
	a: string;
	b: Byte;

	begin
	AStream.Position:= 0;
	i:= 0;
	s:= IntToHex(d.Position, 4) + ' ' ;
	a:= '';

	while AStream.Position < AStream.Size do
		begin
		AStream.Read(b, 1);
		s:= s + IntToHex(b, 2) + ' ';
		if  (b < 32)
		or  (b > 126) then
			a:= a + '_'
		else
			a:= a + string(AnsiChar(b));
		Inc(i);

		if  i = 16 then
			begin
			Writeln(s + '  ' + a);
			i:= 0;
			s:= IntToHex(d.Position, 4) + ' ' ;
			a:= '';
			end;
		end;

	if  i > 0  then
		begin
		while i < 16 do
			begin
			s:= s + '   ';
			Inc(i);
			end;

		Writeln(s + '  ' + a);
		end;
	end;

begin
try
	f:= TFileStream.Create(ParamStr(1), fmOpenRead);
	try
		if  f.Read(r, SizeOf(TRIFFHdr)) <> SizeOf(TRIFFHdr) then
			raise Exception.Create('File size too small!');

		if  (r.id <> 'RIFF')
		or  (r.wav <> 'WAVE') then
			raise Exception.Create('File not RIFF file!');

		if  f.Read(w, SizeOf(TWAVEfmt)) <> SizeOf(TWAVEfmt) then
			raise Exception.Create('No WAVEfmt  block!');

		f.Seek(w.size, soFromCurrent);

		d:= TMemoryStream.Create;
		try
			while True do
				begin
				if  f.Read(b, SizeOf(TBlockHdr)) = SizeOf(TBlockHdr) then
					if  b.id <> 'data' then
						begin
						d.Write(b, SizeOf(TBlockHdr));
						d.CopyFrom(f, b.size);
						end
					else
						Break;
				end;

			if  d.Size > 0 then
				begin
				Writeln(#9'Found ' + IntToStr(d.Size) + 'bytes.');
				Writeln;

				DumpHexData(d);

				o:= TFileStream.Create(ParamStr(1) + '.dat', fmCreate);
				try
					d.Position:= 0;
					o.CopyFrom(d, d.Size);

					finally
					o.Free;
					end;
				end;

			finally
			d.Free;
			end;

		finally
		f.Free;
		end;

	except
		on E: Exception do
			Writeln(E.ClassName, ': ', E.Message);
	end;
end.
