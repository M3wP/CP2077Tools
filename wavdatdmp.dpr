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
