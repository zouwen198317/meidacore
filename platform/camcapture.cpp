
#include "camcapture.h"
#include "log/log.h"
#include<sys/time.h>
#define CAM_DEV_NAME "/dev/video"
//whether support HD mode
#define CAPTURE_MODE  0  //V4L2_MODE_HIGHQUALITY  
#define CAM_ROTATE 0

using namespace std;

CamCapture::CamCapture()
{
	log_print(_DEBUG,"CamCapture Create\n");
	mV4l2Fd = -1;
	isStarted = false;
	isConfigured = false;
	needQBuf = false;
	mFourcc = PIX_FMT_YUV420P;
	m_camType = CAM_OV10635;
	gettimeofday(&mStarTv, NULL);
	
	int ret = openDev();
	if(ret >= 0)
		isStarted = true;
	
       memset(&buf, 0, sizeof (buf));                
}

CamCapture::~CamCapture()
{


	log_print(_INFO,"CamCapture Destory\n");
	if(isStarted)
		stopCapture();
	
	struct v4l2_requestbuffers buf_req; 
	memset(&buf_req, 0, sizeof(buf_req));  	
        buf_req.count = 0;
    	buf_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    	buf_req.memory = V4L2_MEMORY_MMAP;
    	if (ioctl(mV4l2Fd, VIDIOC_REQBUFS, &buf_req) < 0)
    	{
        	printf("free request buffers failed\n");
    	}
		
	closeDev();
}

static void print_pixelformat(char *prefix, int val)
{	
	log_print(_INFO,"%s: %c%c%c%c\n", prefix ? prefix : "pixelformat",val & 0xff,(val >> 8) & 0xff,	(val >> 16) & 0xff,(val >> 24) & 0xff);
}

u32 CamCapture::AppFmtToV4l2Fourcc(u32 fmt)
{
	u32 pixelformat = V4L2_PIX_FMT_YUV420M;
	switch(fmt){
	    case PIX_FMT_YUV420P:   ///< Planar YUV 4:2:0 (1 Cr & Cb sample per 2x2 Y samples) (I420)
	    		pixelformat = V4L2_PIX_FMT_YUV420M;
	    break;
	    case PIX_FMT_NV12:      ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = V4L2_PIX_FMT_NV12;
	    break;
	    case PIX_FMT_NV21:      ///< Packed YUV 4:2:0 (separate Y plane, interleaved Cb & Cr planes)
	    		pixelformat = V4L2_PIX_FMT_NV21;
	    break;
	    //case PIX_FMT_YVU420P:   ///< Planar YUV 4:2:0 (1 Cb & Cr sample per 2x2 Y samples) (YV12)
	    		//pixelformat = V4L2_PIX_FMT_YVU420M;
	    break;
	    case PIX_FMT_YUV422:    ///< Packed pixel, Y0 Cb Y1 Cr 
	    		pixelformat = V4L2_PIX_FMT_YUYV;
	    break;
	    case PIX_FMT_RGB24:     ///< Packed pixel, 3 bytes per pixel, RGBRGB...
	    		pixelformat = V4L2_PIX_FMT_RGB24;
	    break;
	    case PIX_FMT_BGR24:     ///< Packed pixel, 3 bytes per pixel, BGRBGR...
	    		pixelformat = V4L2_PIX_FMT_BGR24;
	    break;
	    case PIX_FMT_YUV422P:   ///< Planar YUV 4:2:2 (1 Cr & Cb sample per 2x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV422P;
	    break;
	    case PIX_FMT_YUV444P:  ///< Planar YUV 4:4:4 (1 Cr & Cb sample per 1x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV444;
	    break;
	   //  case PIX_FMT_RGBA32,    ///< Packed pixel, 4 bytes per pixel, BGRABGRA..., stored in cpu endianness
	   // case PIX_FMT_BGRA32:    ///< Packed pixel, 4 bytes per pixel, ARGBARGB...
	   // case PIX_FMT_ARGB32,    ///< Packed pixel, 4 bytes per pixel, ABGRABGR..., stored in cpu endianness
	   // case PIX_FMT_ABGR32,    ///< Packed pixel, 4 bytes per pixel, RGBARGBA...
	    case PIX_FMT_RGB32:    ///< Packed pixel, 4 bytes per pixel, BGRxBGRx..., stored in cpu endianness
	    		pixelformat = V4L2_PIX_FMT_RGB32;
	    break;
	    //case PIX_FMT_xRGB32,    ///< Packed pixel, 4 bytes per pixel, xBGRxBGR..., stored in cpu endianness
	    case PIX_FMT_BGR32:    ///< Packed pixel, 4 bytes per pixel, xRGBxRGB...
	    		pixelformat = V4L2_PIX_FMT_BGR32;
	    break;
	    //case PIX_FMT_BGRx32,    ///< Packed pixel, 4 bytes per pixel, RGBxRGBx...
	    case PIX_FMT_YUV410P:   ///< Planar YUV 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV410;
	    break;
	    case PIX_FMT_YVU410P:   ///< Planar YVU 4:1:0 (1 Cr & Cb sample per 4x4 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YVU410;
	    break;
	    case PIX_FMT_YUV411P:   ///< Planar YUV 4:1:1 (1 Cr & Cb sample per 4x1 Y samples)
	    		pixelformat = V4L2_PIX_FMT_YUV411P;
	    break;
	    //case PIX_FMT_Y800,      ///< 8 bit Y plane (range [16-235])
	    case PIX_FMT_Y16:       ///< 16 bit Y plane (little endian)
	    		pixelformat = V4L2_PIX_FMT_Y16;
	    break;
	    case PIX_FMT_RGB565:    ///< always stored in cpu endianness 
	    		pixelformat = V4L2_PIX_FMT_RGB565;
	    break;
	    case PIX_FMT_RGB555:    ///< always stored in cpu endianness, most significant bit to 1 
	    		pixelformat = V4L2_PIX_FMT_RGB555;
	    break;
	    case PIX_FMT_GRAY8:
	    		pixelformat = V4L2_PIX_FMT_GREY;
	    break;
	    //case PIX_FMT_GRAY16_L,
	    //case PIX_FMT_GRAY16_B,
	    //case PIX_FMT_MONOWHITE, ///< 0 is white 
	    //case PIX_FMT_MONOBLACK, ///< 0 is black 
	    case PIX_FMT_PAL8:      ///< 8 bit with RGBA palette 
		    		pixelformat = V4L2_PIX_FMT_PAL8;
	    break;
    
	    //case PIX_FMT_YUVJ420P,  ///< Planar YUV 4:2:0 full scale (jpeg)
	    //case PIX_FMT_YUVJ422P,  ///< Planar YUV 4:2:2 full scale (jpeg)
	    //case PIX_FMT_YUVJ444P,  ///< Planar YUV 4:4:4 full scale (jpeg)
	    //case PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing(xvmc_render.h)
	    //case PIX_FMT_XVMC_MPEG2_IDCT,
	    case PIX_FMT_UYVY422:   ///< Packed pixel, Cb Y0 Cr Y1 
		    		pixelformat = V4L2_PIX_FMT_UYVY;
	    break;
	    case PIX_FMT_YVYU422:   ///< Packed pixel, Y0 Cr Y1 Cb 
		    		pixelformat = V4L2_PIX_FMT_YVYU;
	    break;
	    //case PIX_FMT_UYVY411,   ///< Packed pixel, Cb Y0 Y1 Cr Y2 Y3
	    //case PIX_FMT_V308,      ///< Packed pixel, Y0 Cb Cr

	    //case PIX_FMT_AYUV4444,  ///< Packed pixel, A0 Y0 Cb Cr
	    //case PIX_FMT_YUVA420P,   ///< Planar YUV 4:4:2:0 (1 Cr & Cb sample per 2x2 Y & A samples) (A420)
	    default: //PIX_FMT_YUV420P
	    	log_print(_ERROR,"fmt 0x%x not define, default to V4L2_PIX_FMT_YUV420M\n",fmt);
		mFourcc = PIX_FMT_YUV420P;
		return pixelformat;
	    break;
			
	}
	mFourcc = fmt;
	return pixelformat;
}

int CamCapture::configure(vidCapParm *pParm)
{
	struct v4l2_format fmt;	
	struct v4l2_streamparm parm;	
	struct v4l2_fmtdesc fmtdesc;	
	struct v4l2_frmivalenum frmival;	
	log_print(_DEBUG,"CamCapture configure mV4l2Fd %d \n",mV4l2Fd);

	if(mV4l2Fd <0)
		return -1;

	fmtdesc.index = 0;
	while (ioctl(mV4l2Fd, VIDIOC_ENUM_FMT, &fmtdesc) >= 0) {		
		print_pixelformat((char*)"pixelformat (output by camera)",				
			fmtdesc.pixelformat);		
		frmival.index = 0;		
		frmival.pixel_format = fmtdesc.pixelformat;		
		frmival.width = pParm->width;		
		frmival.height = pParm->height;		
		while (ioctl(mV4l2Fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {			
			log_print(_INFO,"VIDIOC_ENUM_FRAMEINTERVALS %.3f fps\n", 1.0 * frmival.discrete.denominator/ frmival.discrete.numerator);			
			frmival.index++;		
		}		
		fmtdesc.index++;	
	}	
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	parm.parm.capture.capturemode = CAPTURE_MODE;	
	if(m_camType == CAM_OV10635){
		if(pParm->fps != 25 && pParm->fps !=30)
		{
			pParm->fps = 25;
			log_print(_INFO," CamCapture::configure CAM_OV10635 Only support 25/30 fps, default 25\n");
		}
	}
	parm.parm.capture.timeperframe.denominator = pParm->fps;	
	parm.parm.capture.timeperframe.numerator = 1;	
	if (ioctl(mV4l2Fd, VIDIOC_S_PARM, &parm) < 0){	        
		log_print(_ERROR," CamCapture::configure VIDIOC_S_PARM fps %d failed\n",pParm->fps);	        
		return -1;	
	}	

	if(m_camType == CAM_OV10635 ){
		struct v4l2_crop crop;
		int camOv10635Input = 1; //can't be change
		if ( (ioctl(mV4l2Fd, VIDIOC_S_INPUT, &camOv10635Input) < 0))
		{
			printf("VIDIOC_S_INPUT failed\n");
			//return -1;
		}

		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(mV4l2Fd, VIDIOC_G_CROP, &crop) < 0)
		{
			printf("VIDIOC_G_CROP failed\n");
			//return -1;
		}
		struct v4l2_cropcap cropcap;
		memset(&cropcap,0,sizeof(v4l2_cropcap));
		cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if(ioctl(mV4l2Fd,VIDIOC_CROPCAP,&cropcap) < 0)
			log_print(_ERROR,"camera not suppose cropcap.\n");
		else log_print(_DEBUG,"camera suppose cropcap.\n");
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c.width = pParm->width;
		crop.c.height = pParm->height;
		crop.c.top = 0;
		crop.c.left = 0;
		if (ioctl(mV4l2Fd, VIDIOC_S_CROP, &crop) < 0)
		{
			printf("VIDIOC_S_CROP failed\n");
			//return -1;
		}
	}
	
	memset(&fmt, 0, sizeof(fmt));        
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	fmt.fmt.pix.pixelformat = AppFmtToV4l2Fourcc(pParm->fourcc);	
	print_pixelformat((char*)"pixelformat (output by v4l)", fmt.fmt.pix.pixelformat);        
	fmt.fmt.pix.width = pParm->width;        
	fmt.fmt.pix.height = pParm->height;       
	if (ioctl(mV4l2Fd, VIDIOC_S_FMT, &fmt) < 0) {                
		printf("set format failed\n");                
		return -1;        
	}      
	 
	//set fps parms again
	if (ioctl(mV4l2Fd, VIDIOC_S_PARM, &parm) < 0){	        
		log_print(_ERROR," CamCapture::configure VIDIOC_S_PARM fps %d failed\n",pParm->fps);	        
		return -1;	
	}	


  	if(m_camType == CAM_OV10635 ){
	      struct v4l2_control ctrl;
	        // Set rotation
	        ctrl.id = V4L2_CID_PRIVATE_BASE + 0;
	        ctrl.value = CAM_ROTATE;//  degree
	        if (ioctl(mV4l2Fd, VIDIOC_S_CTRL, &ctrl) < 0)
	        {
	                printf("set ctrl failed\n");
	                //return 0;
	        }
  	}

/*
	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	
	parm.parm.capture.capturemode = CAPTURE_MODE;	
	parm.parm.capture.timeperframe.denominator = 1;	
	parm.parm.capture.timeperframe.numerator = 1;	
	if (ioctl(mV4l2Fd, VIDIOC_G_PARM, &parm) < 0){	        
		log_print(_INFO," CamCapture::configure VIDIOC_G_PARM failed\n");	        
		return -1;	
	}	
	log_print(_INFO," CamCapture::configure fps %d\n",parm.parm.capture.timeperframe.denominator);	        
*/


        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mV4l2Fd, VIDIOC_G_FMT, &fmt) < 0) {
                log_print(_ERROR, "get format failed\n");
                return -1;
        } else {
                log_print(_INFO, "Width = %d\t Height = %d\n", fmt.fmt.pix.width, fmt.fmt.pix.height);
                log_print(_INFO, "Image size = %d\n", fmt.fmt.pix.sizeimage);
                log_print(_INFO, "pixelformat = %d\n", fmt.fmt.pix.pixelformat);
        }

	
	memset(&req, 0, sizeof (req));       
	req.count = CAM_BUFFER_NUM;       
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;       
	req.memory = V4L2_MEMORY_MMAP;       
	if (ioctl(mV4l2Fd, VIDIOC_REQBUFS, &req) < 0) {               
		log_print(_ERROR," CamCapture::configure v4l_capture_setup: VIDIOC_REQBUFS failed\n");            
		return -1;      
	}
	mWidth = pParm->width; 
	mHeight = pParm->height;
	isConfigured = true;

	log_print(_DEBUG,"CamCapture::configure OK\n");
	
	return 0;
}

u64 CamCapture::getBufferPts(struct v4l2_buffer * buf)
{
	u64 pts =0,pts_sec=0; //ms
	struct  timeval    tv;
#ifdef V4L2
	switch (buf->flags & V4L2_BUF_FLAG_TIMESTAMP_MASK)
#else
	switch (buf->flags)
#endif
	{
#ifdef V4L2
		case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC: //start from 0 when v4l2 streamon
		//	pts = (buf->timestamp.tv_sec * 1000000) + buf->timestamp.tv_usec;
		//break;       
		case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
#endif
		default:
			gettimeofday(&tv, NULL);
			pts_sec = tv.tv_sec;
			//log_print(_DEBUG," CamCapture:: capture gettimeofday to get PTS %lld\n",pts);
			pts = ((pts_sec - mStarTv.tv_sec) * 1000) + tv.tv_usec/1000; //ms
			if(pts_sec < mStarTv.tv_sec) {
				log_print(_ERROR,"CamCapture:: capture pts error. pts_sec %llu, mStarTv.tv_sec %llu\n",pts_sec, mStarTv.tv_sec);
				mStarTv.tv_sec = pts_sec;
			}
			//pts = ((pts_sec - mStarTv.tv_sec) * 1000) + tv.tv_usec/1000; //ms
			//pts = mdate ();
		break;
	}
	return pts;
}


int CamCapture::capture(ImgBlk *img)
{
	int index = 0;
	//log_print(_DEBUG," CamCapture:: capture \n");
	if(img == NULL) {
		log_print(_ERROR," CamCapture:: capture img == NULL\n");
		return -1;
	}
	if(mV4l2Fd <0)
		return -1;


#if 0	
	//if(gCaptCnt < 50) {	
		//if(needQBuf) {
		if(mBufIdQueue.size() >0){
			buf = mBufIdQueue.front();
			mBufIdQueue.pop();
			//log_print(_DEBUG," CamCapture:: capture VIDIOC_QBUF,buf index %d\n",buf.index);
			if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {                              
				log_print(_INFO," CamCapture:: capture VIDIOC_QBUF failed\n");                        		
				return -1;    
			}            
			needQBuf= false;
		}
	       //memset(&buf, 0, sizeof (buf));                
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;               
		buf.memory = V4L2_MEMORY_MMAP;               
		log_print(_DEBUG," CamCapture:: capture VIDIOC_DQBUF\n");
		if (ioctl (mV4l2Fd, VIDIOC_DQBUF, &buf) < 0)	{                     
			log_print(_INFO," CamCapture:: capture VIDIOC_DQBUF failed.\n");  
			return -1;
		}
		mBufIdQueue.push(buf);
#else
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;               
		buf.memory = V4L2_MEMORY_MMAP;               
		//log_print(_DEBUG," CamCapture:: capture VIDIOC_DQBUF\n");
		if (ioctl (mV4l2Fd, VIDIOC_DQBUF, &buf) < 0)	{                     
			log_print(_ERROR," CamCapture:: capture VIDIOC_DQBUF failed.\n");  
			return -1;
		}
		if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {                              
			log_print(_ERROR," CamCapture:: capture VIDIOC_QBUF failed\n");                        		
			return -1;    
		}            


#endif
	index = buf.index;
	needQBuf= true;
	if(index < CAM_BUFFER_NUM) {
		//log_print(_DEBUG," CamCapture:: capture buf.index %d\n", index);
	} else {
		//log_print(_DEBUG," CamCapture:: capture buf.index %d bigger than >= %d\n", index,CAM_BUFFER_NUM);
		index = index - CAM_BUFFER_NUM;
		//return -1;
	}

	img->pts = getBufferPts(&buf);
	
	img->fourcc = mFourcc;
    	img->height = mHeight;
	img->width = mWidth;
	img->size = buf.bytesused;
	
	img->vaddr = buffers[index].start ;
	img->paddr = buffers[index].offset;
	if(buf.m.offset != buffers[index].offset){
		log_print(_ERROR,"CamCapture::capture buf.m.offset 0x%x, buffers[index].offset 0x%x\n",buf.m.offset , buffers[index].offset);
	}
	//log_print(_DEBUG,"CamCapture::capture IMG %d x %d fourcc=0x%x, size=%d, vaddr=0x%x, paddr=0x%x, pts %llu\n",
		//img->width,img->height,img->fourcc,img->size ,(unsigned int)img->vaddr ,(unsigned int)img->paddr ,img->pts);
	return img->size;

}

int CamCapture::startCapture()
{
        unsigned int i;
        struct v4l2_buffer buf;
        enum v4l2_buf_type type;
	log_print(_DEBUG,"CamCapture startCapture mV4l2Fd %d \n",mV4l2Fd);
	if(mV4l2Fd <0)
		return -1;

        for (i = 0; i < CAM_BUFFER_NUM; i++)
        {
                memset(&buf, 0, sizeof (buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
                if (ioctl(mV4l2Fd, VIDIOC_QUERYBUF, &buf) < 0)
                {
                        log_print(_ERROR," CamCapture::startCapture VIDIOC_QUERYBUF error\n");
                        return -1;
                }

                buffers[i].length = buf.length;
                buffers[i].offset = (size_t) buf.m.offset;
                buffers[i].start = (unsigned char *)mmap (NULL, buffers[i].length,
                    			 PROT_READ | PROT_WRITE, MAP_SHARED,
                    			 mV4l2Fd, buffers[i].offset);
		memset(buffers[i].start, 0x00, buffers[i].length);
		log_print(_DEBUG,"CamCapture buffers info  start 0x%x, offset 0x%x,length %d\n",(unsigned int)(buffers[i].start), buffers[i].offset,buffers[i].length);
        }

        for (i = 0; i < CAM_BUFFER_NUM; i++)
        {
                memset(&buf, 0, sizeof (buf));
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;
		  buf.m.offset = buffers[i].offset;

                if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {
                        log_print(_ERROR," CamCapture::startCapture VIDIOC_QBUF error\n");
                        return -1;
                }
        }

	log_print(_DEBUG,"CamCapture VIDIOC_STREAMON \n");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl (mV4l2Fd, VIDIOC_STREAMON, &type) < 0) {
                 log_print(_ERROR," CamCapture::startCapture VIDIOC_STREAMON error\n");
                return -1;
        }
	log_print(_DEBUG,"CamCapture VIDIOC_STREAMON OK\n");
	 isStarted = true;
        return 0;
}

int CamCapture::stopCapture()
{
       enum v4l2_buf_type type;        
	log_print(_DEBUG,"CamCapture stopCapture mV4l2Fd %d \n",mV4l2Fd);
	if(mV4l2Fd <0)
		return -1;
	if(needQBuf) {
		log_print(_DEBUG,"CamCapture stopCapture VIDIOC_QBUF mV4l2Fd %d \n",mV4l2Fd);
		if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {                              
			log_print(_ERROR," CamCapture::stopCapture VIDIOC_QBUF failed\n");                        		
			//return -1;    
		}            
		needQBuf= false;
	}		
	log_print(_DEBUG,"CamCapture stopCapture VIDIOC_STREAMOFF mV4l2Fd %d \n",mV4l2Fd);
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;        
	int ret = ioctl (mV4l2Fd, VIDIOC_STREAMOFF, &type);
	log_print(_DEBUG,"CamCapture stopCapture VIDIOC_STREAMOFF ret %d \n",ret);


	isStarted = false;
	return ret;
}

int CamCapture::openDev()
{
	struct v4l2_capability cap;	
	struct v4l2_dbg_chip_ident chip;

	char camName[128]= "";
	int i=0;
	log_print(_DEBUG,"CamCapture openDev\n");

	for(i=0;i<10;i++){
		memset(camName,0,128);
		sprintf(camName,"%s%d",CAM_DEV_NAME,i);
		if ((mV4l2Fd = open(camName, O_RDWR, 0)) < 0) {		
			log_print(_ERROR," CamCapture::openDev unable to open %s for capture device.\n", camName);	
			continue ;
		}	
		break;
	}
	if(mV4l2Fd <0){
		log_print(_ERROR," CamCapture::openDev unable to open any capture device.\n");	
		return -1;
	}

	if (ioctl(mV4l2Fd, VIDIOC_DBG_G_CHIP_IDENT, &chip))
	{
               log_print(_ERROR,"CamCapture::openDev VIDIOC_DBG_G_CHIP_IDENT failed.\n");
		  m_camType = CAM_UVC;
                //return -1;
	} else {
		log_print(_DEBUG,"CamCapture sensor chip is %s\n", chip.match.name);
        }

	
	if (ioctl(mV4l2Fd, VIDIOC_QUERYCAP, &cap) == 0) {		
		if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {			
			log_print(_DEBUG," CamCapture::openDev  Found v4l2 capture device %s.\n", camName);			
			return mV4l2Fd;		
		}	
	} else {		
		close(mV4l2Fd);	
		mV4l2Fd = -1;
		log_print(_ERROR," CamCapture::openDev  unable to open %s for capture cabality device.\n", camName);	
		return -1;
	}
	return -1;
}

int CamCapture::closeDev()
{
	log_print(_INFO,"CamCapture closeDev\n");
	if(mV4l2Fd >=0){
		close(mV4l2Fd);	
		mV4l2Fd = -1;
	}
	return 0;
}

fbuffer_info* CamCapture::get_buffer_info(fbuffer_info* info)
{
	if(info != NULL){
		//unsigned int * p = (unsigned int*)info;
		int i;
		for(i = 0;i < CAM_BUFFER_NUM;++i) {//CAM_BUFFER_NUM == 3
			info->addr[i] = buffers[i].offset;
		}
		info->length = buffers[0].length;
		info->num = i;
	}
	return info;
}

