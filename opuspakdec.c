/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE libopusfile SOFTWARE CODEC SOURCE CODE. *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE libopusfile SOURCE CODE IS (C) COPYRIGHT 1994-2020           *
 * by the Xiph.Org Foundation and contributors https://xiph.org/    *
 *                                                                  *
 ********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*For fileno()*/
#if !defined(_POSIX_SOURCE)
# define _POSIX_SOURCE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opusfile.h>
#if defined(_WIN32)
# include "win32utf8.h"
# undef fileno
# define fileno _fileno
#endif

#define EX_ID_RIFF ((uint32_t)0x46464952)
#define EX_ID_DATA ((uint32_t)0x61746164)
#define EX_ID_OGGS ((uint32_t)0x5367674f)

char lsthdr[1024];
int lsthdrlen = 0;
int lstreadp = 0;
ogg_int64_t   duration = 0;

static int ex_prepheader(void* _stream) {
    char rb[4];
    uint32_t id;
    int cnt;
    unsigned char* p;

    lstreadp = 0;

    if (fread(&rb, 1, 4, _stream) != 4) {
        return -1;
    }

    id = rb[0] | (rb[1] << 8) | (rb[2] << 16) | (rb[3] << 24);

    if (id == EX_ID_RIFF) {
        strncpy(&lsthdr, &rb, 4);
        lsthdrlen = 4;

        cnt = 0;
        while (cnt < 3) {
            rb[0] = rb[1];
            rb[1] = rb[2];
            rb[2] = rb[3];

            if (fread(&rb[3], 1, 1, _stream) != 1)
                return -2;

            lsthdr[lsthdrlen] = rb[3];
            lsthdrlen++;

            id = rb[0] | (rb[1] << 8) | (rb[2] << 16) | (rb[3] << 24);

            if (id == EX_ID_OGGS)
                cnt++;
        }

        lsthdr[lsthdrlen] = 0;

        id = EX_ID_DATA;

        for (p = lsthdr; p++; (p - lsthdr) < lsthdrlen) {
            if (memicmp(p, &id, 4) == 0)
                break;
        }

        if ((p - lsthdr) == lsthdrlen)
            return -3;

        id = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);

        duration = id >> 1;

    } else if (id == EX_ID_OGGS) {

    }
    else
        return -4;

    return 0;
}

//fuck had to copy even this
# define EX_INT64_MAX (2*(((ogg_int64_t)1<<62)-1)|1)

static int ex_fread(void* _stream, unsigned char* _ptr, int _buf_size) {
    FILE* stream;
    size_t  ret;
    /*Check for empty read.*/
    if (_buf_size <= 0)return 0;
    stream = (FILE*)_stream;
    
//  ret = fread(_ptr, 1, _buf_size, stream);
//  ret = fread(_ptr, 1, 1, stream);

    if ((lsthdrlen > 0) && (lstreadp < lsthdrlen)) {
        ret = 1;
        _ptr[0] = lsthdr[lstreadp];
        lstreadp++;
    }
    else {
        ret = fread(_ptr, 1, 1, stream);
    }

//  OP_ASSERT(ret <= (size_t)_buf_size);
    /*If ret==0 and !feof(stream), there was a read error.*/
    return ret > 0 || feof(stream) ? (int)ret : OP_EREAD;
}

static int ex_fseek(void* _stream, opus_int64 _offset, int _whence) {
#if defined(_WIN32)
    /*_fseeki64() is not exposed until MSVCRT80.
      This is the default starting with MSVC 2005 (_MSC_VER>=1400), but we want
       to allow linking against older MSVCRT versions for compatibility back to
       XP without installing extra runtime libraries.
      i686-pc-mingw32 does not have fseeko() and requires
       __MSVCRT_VERSION__>=0x800 for _fseeki64(), which screws up linking with
       other libraries (that don't use MSVCRT80 from MSVC 2005 by default).
      i686-w64-mingw32 does have fseeko() and respects _FILE_OFFSET_BITS, but I
       don't know how to detect that at compile time.
      We could just use fseeko64() (which is available in both), but it's
       implemented using fgetpos()/fsetpos() just like this code, except without
       the overflow checking, so we prefer our version.*/
    opus_int64 pos;
    /*We don't use fpos_t directly because it might be a struct if __STDC__ is
       non-zero or _INTEGRAL_MAX_BITS < 64.
      I'm not certain when the latter is true, but someone could in theory set
       the former.
      Either way, it should be binary compatible with a normal 64-bit int (this
       assumption is not portable, but I believe it is true for MSVCRT).*/
//  OP_ASSERT(sizeof(pos) == sizeof(fpos_t));
    /*Translate the seek to an absolute one.*/
    if (_whence == SEEK_CUR) {
        int ret;
        ret = fgetpos((FILE*)_stream, (fpos_t*)&pos);
        if (ret)return ret;
    }
    else if (_whence == SEEK_END)pos = _filelengthi64(_fileno((FILE*)_stream));
    else if (_whence == SEEK_SET)pos = 0;
    else return -1;
    /*Check for errors or overflow.*/
    if (pos < 0 || _offset<-pos || _offset>EX_INT64_MAX - pos)return -1;
    pos += _offset;
    return fsetpos((FILE*)_stream, (fpos_t*)&pos);
#else
    /*This function actually conforms to the SUSv2 and POSIX.1-2001, so we prefer
       it except on Windows.*/
    return fseeko((FILE*)_stream, (off_t)_offset, _whence);
#endif
}

static opus_int64 ex_ftell(void* _stream) {
#if defined(_WIN32)
    /*_ftelli64() is not exposed until MSVCRT80, and ftello()/ftello64() have
       the same problems as fseeko()/fseeko64() in MingW.
      See above for a more detailed explanation.*/
    opus_int64 pos;
//  OP_ASSERT(sizeof(pos) == sizeof(fpos_t));
    return fgetpos((FILE*)_stream, (fpos_t*)&pos) ? -1 : pos;
#else
    /*This function actually conforms to the SUSv2 and POSIX.1-2001, so we prefer
       it except on Windows.*/
    return ftello((FILE*)_stream);
#endif
}

static const OpusFileCallbacks EX_FILE_CALLBACKS = {
  ex_fread,
  NULL,//ex_fseek,
  ex_ftell,
  (op_close_func)NULL// fclose
};

static void print_duration(FILE *_fp,ogg_int64_t _nsamples,int _frac){
  ogg_int64_t seconds;
  ogg_int64_t minutes;
  ogg_int64_t hours;
  ogg_int64_t days;
  ogg_int64_t weeks;
  _nsamples+=_frac?24:24000;
  seconds=_nsamples/48000;
  _nsamples-=seconds*48000;
  minutes=seconds/60;
  seconds-=minutes*60;
  hours=minutes/60;
  minutes-=hours*60;
  days=hours/24;
  hours-=days*24;
  weeks=days/7;
  days-=weeks*7;
  if(weeks)fprintf(_fp,"%liw",(long)weeks);
  if(weeks||days)fprintf(_fp,"%id",(int)days);
  if(weeks||days||hours){
    if(weeks||days)fprintf(_fp,"%02ih",(int)hours);
    else fprintf(_fp,"%ih",(int)hours);
  }
  if(weeks||days||hours||minutes){
    if(weeks||days||hours)fprintf(_fp,"%02im",(int)minutes);
    else fprintf(_fp,"%im",(int)minutes);
    fprintf(_fp,"%02i",(int)seconds);
  }
  else fprintf(_fp,"%i",(int)seconds);
  if(_frac)fprintf(_fp,".%03i",(int)(_nsamples/48));
  fprintf(_fp,"s");
}

static void print_size(FILE *_fp,opus_int64 _nbytes,int _metric,
 const char *_spacer){
  static const char SUFFIXES[7]={' ','k','M','G','T','P','E'};
  opus_int64 val;
  opus_int64 den;
  opus_int64 round;
  int        base;
  int        shift;
  base=_metric?1000:1024;
  round=0;
  den=1;
  for(shift=0;shift<6;shift++){
    if(_nbytes<den*base-round)break;
    den*=base;
    round=den>>1;
  }
  val=(_nbytes+round)/den;
  if(den>1&&val<10){
    if(den>=1000000000)val=(_nbytes+(round/100))/(den/100);
    else val=(_nbytes*100+round)/den;
    fprintf(_fp,"%li.%02i%s%c",(long)(val/100),(int)(val%100),
     _spacer,SUFFIXES[shift]);
  }
  else if(den>1&&val<100){
    if(den>=1000000000)val=(_nbytes+(round/10))/(den/10);
    else val=(_nbytes*10+round)/den;
    fprintf(_fp,"%li.%i%s%c",(long)(val/10),(int)(val%10),
     _spacer,SUFFIXES[shift]);
  }
  else fprintf(_fp,"%li%s%c",(long)val,_spacer,SUFFIXES[shift]);
}

static void put_le32(unsigned char *_dst,opus_uint32 _x){
  _dst[0]=(unsigned char)(_x&0xFF);
  _dst[1]=(unsigned char)(_x>>8&0xFF);
  _dst[2]=(unsigned char)(_x>>16&0xFF);
  _dst[3]=(unsigned char)(_x>>24&0xFF);
}

/*Make a header for a 48 kHz, stereo, signed, 16-bit little-endian PCM WAV.*/
static void make_wav_header(unsigned char _dst[44],ogg_int64_t _duration){
  /*The chunk sizes are set to 0x7FFFFFFF by default.
    Many, though not all, programs will interpret this to mean the duration is
     "undefined", and continue to read from the file so long as there is actual
     data.*/
  static const unsigned char WAV_HEADER_TEMPLATE[44]={
    'R','I','F','F',0xFF,0xFF,0xFF,0x7F,
    'W','A','V','E','f','m','t',' ',
    0x10,0x00,0x00,0x00,0x01,0x00,0x02,0x00,
    0x80,0xBB,0x00,0x00,0x00,0xEE,0x02,0x00,
    0x04,0x00,0x10,0x00,'d','a','t','a',
    0xFF,0xFF,0xFF,0x7F
  };
  memcpy(_dst,WAV_HEADER_TEMPLATE,sizeof(WAV_HEADER_TEMPLATE));
  if(_duration>0){
    if(_duration>0x1FFFFFF6){
      fprintf(stderr,"WARNING: WAV output would be larger than 2 GB.\n");
      fprintf(stderr,
       "Writing non-standard WAV header with invalid chunk sizes.\n");
    }
    else{
      opus_uint32 audio_size;
      audio_size=(opus_uint32)(_duration*4);
      put_le32(_dst+4,audio_size+36);
      put_le32(_dst+40,audio_size);
    }
  }
}

int main(int _argc,const char **_argv){
  OggOpusFile  *of;
  OpusFileCallbacks* cb;
  FILE* sf = NULL;
  FILE* wf = NULL;
//ogg_int64_t   duration;
  unsigned char wav_header[44];
  int           ret;
  int           is_ssl;
  int           output_seekable;
  char outname[250];
  char* p;
  int fc = 0;
  long t;

#if defined(_WIN32)
  win32_utf8_setup(&_argc,&_argv);
#endif
  if (_argc != 2){
    fprintf(stderr,"Usage: %s <file.opus>\n",_argv[0]);
    return EXIT_FAILURE;
  }
  is_ssl=0;

  strcpy(&outname, _argv[1]);

  p = strchr(&outname, '.');
  if ((int)(p - outname) > 240) {
      fprintf(stderr, "Usage: %s <file.opuspak>\n", _argv[0]);
      return EXIT_FAILURE;
  }

  sprintf(p, ".%3.3d.wav", fc);

  wf = fopen(&outname, "wb");
//of = op_open_file(_argv[1], &ret);
  sf = fopen(_argv[1], "rb");
  if (ex_prepheader(sf) < 0)
      return EXIT_FAILURE;

  cb = &EX_FILE_CALLBACKS;

  of = op_open_callbacks(sf, cb, NULL, 0, &ret);

  if(of==NULL){
    fprintf(stderr,"Failed to open file '%s': %i\n",_argv[1],ret);
    return EXIT_FAILURE;
  }
  
  //duration=0;
  
  output_seekable=fseek(wf,0,SEEK_CUR)!=-1;
  
  if(op_seekable(of)){
    opus_int64  size;
    fprintf(stderr,"Total number of links: %i\n",op_link_count(of));
//  duration=op_pcm_total(of,-1);
    fprintf(stderr,"Total duration: ");
    print_duration(stderr,duration,3);
    fprintf(stderr," (%li samples @ 48 kHz)\n",(long)duration);
    size=op_raw_total(of,-1);
    fprintf(stderr,"Total size: ");
    print_size(stderr,size,0,"");
    fprintf(stderr,"\n");
  }
  else if(!output_seekable){
    fprintf(stderr,"WARNING: Neither input nor output are seekable.\n");
    fprintf(stderr,
     "Writing non-standard WAV header with invalid chunk sizes.\n");
  }
  make_wav_header(wav_header,duration);
  if(!fwrite(wav_header,sizeof(wav_header),1,wf)){
    fprintf(stderr,"Error writing WAV header: %s\n",strerror(errno));
    ret=EXIT_FAILURE;
  }
  else{
    ogg_int64_t pcm_offset;
    ogg_int64_t pcm_print_offset;
    ogg_int64_t nsamples;
    opus_int32  bitrate;
    int         prev_li;
    prev_li=-1;
    nsamples=0;
    pcm_offset=op_pcm_tell(of);
    if(pcm_offset!=0){
      fprintf(stderr,"Non-zero starting PCM offset: %li\n",(long)pcm_offset);
    }
    pcm_print_offset=pcm_offset-48000;
    bitrate=0;
    for(;;){
      ogg_int64_t   next_pcm_offset;
      opus_int16    pcm[120*48*2];
      unsigned char out[120*48*2*2];
      int           li;
      int           si;
      
      /*Although we would generally prefer to use the float interface, WAV
         files with signed, 16-bit little-endian samples are far more
         universally supported, so that's what we output.*/
      
      ret = op_read_stereo(of, pcm, 2);//sizeof(pcm) / sizeof(*pcm));

      if(ret==OP_HOLE){
        fprintf(stderr,"\nHole detected! Corrupt file segment?\n");
        continue;
      }
      else if(ret<0){
        fprintf(stderr,"\nError decoding '%s': %i\n",_argv[1],ret);
        if(is_ssl)fprintf(stderr,"Possible truncation attack?\n");
        ret=EXIT_FAILURE;
        break;
      }

//>>>>

//      next_pcm_offset=op_pcm_tell(of);
//      if(pcm_offset+ret!=next_pcm_offset){
//        fprintf(stderr,"\nPCM offset gap! %li+%i!=%li\n",
//         (long)pcm_offset,ret,(long)next_pcm_offset);
//      }
//      pcm_offset=next_pcm_offset;

      if(ret<=0){
        ret=EXIT_SUCCESS;
        break;
      }

      /*Ensure the data is little-endian before writing it out.*/
      for(si=0;si<2*ret;si++){
        out[2*si+0]=(unsigned char)(pcm[si]&0xFF);
        out[2*si+1]=(unsigned char)(pcm[si]>>8&0xFF);
      }
      if(!fwrite(out,sizeof(*out)*4*ret,1,wf)){
        fprintf(stderr,"\nError writing decoded audio data: %s\n",
         strerror(errno));
        ret=EXIT_FAILURE;
        break;
      }
      nsamples+=ret;

//    prev_li=li;

      if (feof(wf)) {
          fprintf(stderr, "\nAll files extracted.\n");
          ret = EXIT_SUCCESS;
          break;
      }

      if (nsamples >= duration) {
          fprintf(stderr, "\nSkipping to next file...\n");

          make_wav_header(wav_header, nsamples);
          if (fseek(wf, 0, SEEK_SET) ||
              !fwrite(wav_header, sizeof(wav_header), 1, wf)) {
              fprintf(stderr, "Error rewriting WAV header: %s\n", strerror(errno));
              ret = EXIT_FAILURE;
              break;
          }

          op_free(of);
          fclose(wf);

          fc++;
          sprintf(p, ".%3.3d.wav", fc);

//        t = ftell(sf);
          opus_int64 pos;
          t = fgetpos(sf, &pos) ? -1 : pos;
          if (ex_prepheader(sf) < 0)
              return EXIT_FAILURE;

          cb = &EX_FILE_CALLBACKS;

          wf = fopen(&outname, "wb");

          make_wav_header(wav_header, duration);
          if (!fwrite(wav_header, sizeof(wav_header), 1, wf)) {
              fprintf(stderr, "Error writing WAV header: %s\n", strerror(errno));
              ret = EXIT_FAILURE;
              break;
          }

          of = op_open_callbacks(sf, cb, NULL, 0, &ret);

          if (of == NULL) {
              fprintf(stderr, "\nError attaching next file!\n");
              ret = EXIT_FAILURE;
              break;
          }

//        duration = op_pcm_total(of, -1);
          nsamples = 0;
      }
    }

    if(ret==EXIT_SUCCESS){
      fprintf(stderr,"\nDone: played ");
      print_duration(stderr,nsamples,3);
      fprintf(stderr," (%li samples @ 48 kHz).\n",(long)nsamples);
      if (op_seekable(of) && nsamples != duration) {
          fprintf(stderr, "\nWARNING: "
              "Number of output samples does not match declared file duration.\n");
          if (!output_seekable)fprintf(stderr, "Output WAV file will be corrupt.\n");
      }
      if(output_seekable&&nsamples!=duration){
        make_wav_header(wav_header,nsamples);
        if(fseek(wf,0,SEEK_SET)||
          !fwrite(wav_header,sizeof(wav_header),1,wf)){
          fprintf(stderr,"Error rewriting WAV header: %s\n",strerror(errno));
          ret=EXIT_FAILURE;
        }
      }
    }
  }
  op_free(of);
  return ret;
}
