// .asf fileformat docs from http://divx.euro.ru

typedef struct __attribute__((packed)) {
  unsigned char guid[16];
  unsigned long long size;
} ASF_obj_header_t;

typedef struct __attribute__((packed)) {
  ASF_obj_header_t objh;
  unsigned int cno; // number of subchunks
  unsigned char v1; // unknown (0x01)
  unsigned char v2; // unknown (0x02)
} ASF_header_t;

typedef struct __attribute__((packed)) {
  unsigned char client[16]; // Client GUID
  unsigned long long file_size;
  unsigned long long creat_time; //File creation time FILETIME 8
  unsigned long long packets;    //Number of packets UINT64 8
  unsigned long long end_timestamp; //Timestamp of the end position UINT64 8
  unsigned long long duration;  //Duration of the playback UINT64 8
  unsigned long start_timestamp; //Timestamp of the start position UINT32 4
  unsigned long unk1; //Unknown, maybe reserved ( usually contains 0 ) UINT32 4
  unsigned long flags; //Unknown, maybe flags ( usually contains 2 ) UINT32 4
  unsigned long packetsize; //Size of packet, in bytes UINT32 4
  unsigned long packetsize2; //Size of packet ( confirm ) UINT32 4
  unsigned long frame_size; //Size of uncompressed video frame UINT32 4
} ASF_file_header_t;

typedef struct __attribute__((packed)) {
  unsigned char type[16]; // Stream type (audio/video) GUID 16
  unsigned char concealment[16]; // Audio error concealment type GUID 16
  unsigned long long unk1; // Unknown, maybe reserved ( usually contains 0 ) UINT64 8
  unsigned long type_size; //Total size of type-specific data UINT32 4
  unsigned long stream_size; //Size of stream-specific data UINT32 4
  unsigned short stream_no; //Stream number UINT16 2
  unsigned long unk2; //Unknown UINT32 4
} ASF_stream_header_t;


ASF_header_t asfh;
ASF_obj_header_t objh;
ASF_file_header_t fileh;
ASF_stream_header_t streamh;
unsigned char* asf_packet=NULL;
//int asf_video_id=-1;
int asf_scrambling_h=1;
int asf_scrambling_w=1;
int asf_scrambling_b=1;

int i;

void asf_descrambling(unsigned char *src,int len){
  unsigned char *dst=malloc(len);
  unsigned char *s2=src;
  int i=0,x,y;
  while(len-i>=asf_scrambling_h*asf_scrambling_w*asf_scrambling_b){
//    printf("descrambling! (w=%d  b=%d)\n",w,asf_scrambling_b);
	//i+=asf_scrambling_h*asf_scrambling_w;
	for(x=0;x<asf_scrambling_w;x++)
	  for(y=0;y<asf_scrambling_h;y++){
	    memcpy(dst+i,s2+(y*asf_scrambling_w+x)*asf_scrambling_b,asf_scrambling_b);
		i+=asf_scrambling_b;
	  }
	s2+=asf_scrambling_h*asf_scrambling_w*asf_scrambling_b;
  }
  //if(i<len) memcpy(dst+i,src+i,len-i);
  memcpy(src,dst,i);
  free(dst);
}

char* asf_chunk_type(unsigned char* guid){
  switch(*((unsigned int*)guid)){
    case 0xF8699E40: return "guid_audio_stream";
    case 0xBC19EFC0: return "guid_video_stream";
    case 0x49f1a440: return "guid_audio_conceal_none";
    case 0xbfc3cd50: return "guid_audio_conceal_interleave";
    case 0x75B22630: return "guid_header";
    case 0x75b22636: return "guid_data_chunk";
    case 0x33000890: return "guid_index_chunk";
    case 0xB7DC0791: return "guid_stream_header";
    case 0xD6E229D1: return "guid_header_2_0";
    case 0x8CABDCA1: return "guid_file_header";
  }
  return NULL;
}

int asf_check_header(){
  unsigned char asfhdrguid[16]={0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C};
  stream_read(demuxer->stream,(char*) &asfh,sizeof(asfh)); // header obj
//  for(i=0;i<16;i++) printf(" %02X",temp[i]);printf("\n");
//  for(i=0;i<16;i++) printf(" %02X",asfhdrguid[i]);printf("\n");
  if(memcmp(asfhdrguid,asfh.objh.guid,16)){
    if(verbose) printf("ASF_check: not ASF guid!\n");
    return 0; // not ASF guid
  }
  if(asfh.cno>256){
    if(verbose) printf("ASF_check: invalid subchunks_no %d\n",(int) asfh.cno);
    return 0; // invalid header???
  }
  return 1;
}

int read_asf_header(){
  unsigned char buffer[512];
  
#if 1
  printf("ASF file! (subchunks: %d)\n",asfh.cno);
while(!stream_eof(demuxer->stream)){
  int pos,endpos;
  pos=stream_tell(demuxer->stream);
  stream_read(demuxer->stream,(char*) &objh,sizeof(objh));
  if(stream_eof(demuxer->stream)) break; // EOF
  endpos=pos+objh.size;
//  for(i=0;i<16;i++) printf("%02X ",objh.guid[i]);
  printf("0x%08X  [%s] %d\n",pos, asf_chunk_type(objh.guid),(int) objh.size);
  switch(*((unsigned int*)&objh.guid)){
    case 0xB7DC0791: // guid_stream_header
      stream_read(demuxer->stream,(char*) &streamh,sizeof(streamh));
      printf("stream type: %s\n",asf_chunk_type(streamh.type));
      printf("stream concealment: %s\n",asf_chunk_type(streamh.concealment));
      printf("type: %d bytes,  stream: %d bytes  ID: %d\n",(int)streamh.type_size,(int)streamh.stream_size,(int)streamh.stream_no);
      printf("unk1: %lX  unk2: %X\n",streamh.unk1,streamh.unk2);
      printf("FILEPOS=0x%X\n",stream_tell(demuxer->stream));
      // type-specific data:
      stream_read(demuxer->stream,(char*) buffer,streamh.type_size);
      switch(*((unsigned int*)&streamh.type)){
      case 0xF8699E40: { // guid_audio_stream
        sh_audio_t *sh_audio=&sh_audio_i; // FIXME!
        memcpy(&sh_audio->wf,buffer,streamh.type_size<64?streamh.type_size:64);
        if(verbose>=1) print_wave_header((WAVEFORMATEX*)buffer);
	if((*((unsigned int*)&streamh.concealment))==0xbfc3cd50){
          stream_read(demuxer->stream,(char*) buffer,streamh.stream_size);
          asf_scrambling_h=buffer[0];
          asf_scrambling_w=(buffer[2]<<8)|buffer[1];
          asf_scrambling_b=(buffer[4]<<8)|buffer[3];
  	  asf_scrambling_w/=asf_scrambling_b;
	} else {
	  asf_scrambling_b=asf_scrambling_h=asf_scrambling_w=1;
	}
	printf("ASF audio scrambling: %d x %d x %d\n",asf_scrambling_h,asf_scrambling_w,asf_scrambling_b);
	if(demuxer->audio->id==-1) demuxer->audio->id=streamh.stream_no & 0x7F;
        break;
        }
      case 0xBC19EFC0: { // guid_video_stream
        sh_video_t *sh_video=&sh_video_i; // FIXME!
        memcpy(&sh_video->bih,&buffer[4+4+1+2],sizeof(BITMAPINFOHEADER));
        sh_video->fps=(float)sh_video->video.dwRate/(float)sh_video->video.dwScale;
        sh_video->frametime=(float)sh_video->video.dwScale/(float)sh_video->video.dwRate;
        if(verbose>=1) print_video_header((BITMAPINFOHEADER*)&buffer[4+4+1+2]);
        //asf_video_id=streamh.stream_no & 0x7F;
	if(demuxer->video->id==-1) demuxer->video->id=streamh.stream_no & 0x7F;
        break;
        }
      }
      // stream-specific data:
      // stream_read(demuxer->stream,(char*) buffer,streamh.stream_size);
      break;
//    case 0xD6E229D1: return "guid_header_2_0";
    case 0x8CABDCA1: // guid_file_header
      stream_read(demuxer->stream,(char*) &fileh,sizeof(fileh));
      printf("packets: %d  flags: %d  pack_size: %d  frame_size: %d\n",(int)fileh.packets,(int)fileh.flags,(int)fileh.packetsize,(int)fileh.frame_size);
      asf_packet=malloc(fileh.packetsize); // !!!
      break;
    case 0x75b22636: // guid_data_chunk
      avi_header.movi_start=stream_tell(demuxer->stream)+26;
      avi_header.movi_end=endpos;
      if(verbose>=1) printf("Found movie at 0x%X - 0x%X\n",avi_header.movi_start,avi_header.movi_end);
      break;

//    case 0x33000890: return "guid_index_chunk";

  } // switch GUID
  if(!stream_seek(demuxer->stream,endpos)) break;
} // while EOF

#if 0
if(verbose){
    printf("ASF duration: %d\n",(int)fileh.duration);
    printf("ASF start pts: %d\n",(int)fileh.start_timestamp);
    printf("ASF end pts: %d\n",(int)fileh.end_timestamp);
}
#endif

#endif
return 1;
}

