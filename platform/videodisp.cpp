#include "videodisp.h"
#include <time.h>
#include <sys/time.h>
#include "log/log.h"
#include <linux/mxcfb.h>
#include"parms_dump.h"

VidDisp::VidDisp(vidDispParm Parm,int memoryType)
{
    log_print(_DEBUG,"VidDisp Create memoryType %d\n",memoryType);
    isStarted = false;
    m_num_buffers = 0;
    m_num_enqbuf = 0;
    fliterImg = 0;
    mV4l2Fd = -1;
    m_mem_type = memoryType; //V4L2_MEMORY_USERPTR;V4L2_MEMORY_MMAP

    mVidDispParm = Parm;
    memcpy(devPath, Parm.dev_path, MAX_PATH_NAME_LEN);

    openDev();
    configure(&mVidDispParm);
}

int VidDisp::configure(vidDispParm *pParm)
{
    log_print(_DEBUG,"VidDisp::configure\n");
    struct v4l2_crop crop;
    struct v4l2_rect icrop;

    crop.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    crop.c.top = pParm->crop_top;
    crop.c.left = pParm->crop_left;
    crop.c.width = pParm->crop_width;
    crop.c.height = pParm->crop_height;
//   if( crop.c.width >= gDispIniParm.LCD.width) {  //IPU out video will occur many invalid lines of img to bt656 when strength light on right side of LCD
//      crop.c.width = crop.c.width -8;
//   }  
    if (ioctl(mV4l2Fd, VIDIOC_S_CROP, &crop) < 0) {
        log_print(_ERROR,"VidDisp::configure set crop failed\n");
        printf("####set crop %d,%d,%d,%d,%d\n",crop.c.width,crop.c.height,crop.c.left,crop.c.top,pParm->crop_top);
    }
    log_print(_INFO,"VidDisp::configure set crop %d,%d,%d,%d,%d, fmt %dx%d\n",crop.c.width,crop.c.height,crop.c.left,crop.c.top,pParm->crop_top,pParm->width,pParm->height);
    struct v4l2_control ctrl;
    ctrl.id = V4L2_CID_ROTATE;
    ctrl.value = pParm->rotate;
    if (ioctl(mV4l2Fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        log_print(_ERROR,"VidDisp::configure set ctrl rotate failed\n");
    }

    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    fmt.fmt.pix.width=pParm->width;
    fmt.fmt.pix.height=pParm->height;
    fmt.fmt.pix.pixelformat = AppFmtToV4l2Fourcc(pParm->fourcc);
    //    fmt.fmt.pix.priv = 0;
    icrop.left = 0;
    icrop.top = 0;
    icrop.width = 0;
    icrop.height = 0;
    if(pParm->icrop_width > 0 && pParm->icrop_height> 0) {
        icrop.left = pParm->icrop_left;
        icrop.top = pParm->icrop_top;
        icrop.width = pParm->icrop_width;
        icrop.height = pParm->icrop_height;
    }

    fmt.fmt.pix.priv = (unsigned int) &icrop;
    if (ioctl(mV4l2Fd, VIDIOC_S_FMT, &fmt) < 0) {
        log_print(_ERROR,"VidDisp::configure set format failed\n");
    }
    if (ioctl(mV4l2Fd, VIDIOC_G_FMT, &fmt) < 0) {
        log_print(_ERROR,"VidDisp::configure get format failed\n");
    }
    
}

int VidDisp::addBuffer(unsigned char * vaddr,unsigned int paddr,size_t length)
{       
    log_print(_DEBUG," VidDisp::addBuffer paddr 0x%x \n",paddr);
    if(m_mem_type == V4L2_MEMORY_MMAP) {
        log_print(_ERROR,"VidDisp::addBuffer() V4L2_MEMORY_MMAP can not addBuffer()\n");    
        return -1;
    }
    //unsigned int page_size = getpagesize();
    //page_size = getpagesize();
    if(vaddr == NULL) {
        //printf("add buffer fail\n");
        //return -1;
    }
    buffers[m_num_buffers].start = vaddr;        
    buffers[m_num_buffers].offset = paddr;        
    buffers[m_num_buffers].length = length;
    //buffers[m_num_buffers].length = (length + page_size - 1) & ~(page_size - 1);
    m_num_buffers += 1;
    return 0;
}

int VidDisp::switchBuffers(int amount,ImgBlk *newBuffers[],vidDispParm *pParm)
{   
    log_print(_DEBUG," VidDisp::switchBuffers \n");
    if(!isStarted) {
        printf("VidDisp::switchBuffers() stream have not started\n");       
        return -1;
    }
    for(int i = 0;i<m_num_buffers;i++) {
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = m_mem_type;
        if (ioctl (mV4l2Fd, VIDIOC_DQBUF, &buf) < 0) {
            printf("VidDisp::switchBuffers() VIDIOC_DQBUF failed.\n");
            return -1;
        }
    }
    if(pParm != NULL) {
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl (mV4l2Fd, VIDIOC_STREAMOFF, &type) < 0) {
            printf("VidDisp::switchBuffers() Could not stop stream\n");
            return -1;
        }
        configure(pParm);
        type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        if (ioctl (mV4l2Fd, VIDIOC_STREAMON, &type) < 0) {
            printf("VidDisp::switchBuffers() Could not start stream\n");
            return -1;
        }
    }
    
    m_num_buffers = 0;
    for(int i = 0;i < amount;i++) {
        addBuffer((unsigned char*)newBuffers[i]->vaddr,newBuffers[i]->paddr,newBuffers[i]->size);
    }
    
    for(int i = 0;i < m_num_buffers;i++) {
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = m_mem_type;
        buf.index = i;
        if (ioctl(mV4l2Fd, VIDIOC_QUERYBUF, &buf) < 0) {
            printf("VidDisp::switchBuffers() VIDIOC_QUERYBUF error\n");
            return -1;
        }
        if (m_mem_type == V4L2_MEMORY_USERPTR) {
            buf.m.userptr = (unsigned long) buffers[buf.index].offset;
            buf.length = buffers[buf.index].length;
        }  
        if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {
            printf("VidDisp::switchBuffers() VIDIOC_QBUF error\n");
            return -1;
        }
    }
    log_print(_INFO,"VidDisp::switchBuffers using  %d/%d buffers\n", m_num_buffers,DISP_BUFFER_MAX_NUM);
}

VidDisp::~VidDisp()
{
    log_print(_DEBUG," VidDisp::~VidDisp \n");
   // printf("#########~VidDisp()\n");
    if(isStarted)
        stopStoplay();
        //printf("#########~VidDisp()\n");
    closeDev();
}

int VidDisp::getDispImgBlk()
{
    log_print(_DEBUG," VidDisp::getDispImgBlk \n");
    return 0;
}


int VidDisp::display(ImgBlk *newBuffers)
{
    int ret = -1, times =10;
    //log_print(_DEBUG," VidDisp::display \n");

    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf.memory = m_mem_type;

    //V4L2 incoming queue and outgoing queue must be empty in video output mode when initalising. 
    if(m_num_enqbuf < m_num_buffers)
    {
        log_print(_INFO, "VidDisp::display() enqueue bufnum %d\n", m_num_enqbuf);
        buf.index = m_num_enqbuf;
        buf.m.userptr  = newBuffers->paddr;
        if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {
            log_print(_ERROR,"VidDisp::display() line%d VIDIOC_QBUF failed: %s\n",__LINE__, strerror(errno));
            return -1;
        }
        if(!isStarted) {
            int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
            if (ioctl (mV4l2Fd, VIDIOC_STREAMON, &type) < 0) {
                printf("VidDisp::startDisplay() Could not start stream\n");
                return -1;
            }
            log_print(_DEBUG,"VidDisp::display() stream have started\n");   
            isStarted = true;
        }
        m_num_enqbuf++;
        return 0;
    }

    //log_print(_INFO,"VidDisp::display() paddr 0x%x, pts %lld\n",newBuffers->paddr, newBuffers->pts);
    if(m_mem_type == V4L2_MEMORY_USERPTR) {
        if ((ret = ioctl (mV4l2Fd, VIDIOC_DQBUF, &buf) )< 0) {
            if(errno != EAGAIN)
                log_print(_ERROR,"VidDisp::display() line%d VIDIOC_QBUF failed: %s\n",__LINE__, strerror(errno));
            return -1;      
        }
        
        if(buf.m.userptr  != newBuffers->paddr) {
            log_print(_WARN,"VidDisp::display() paddr 0x%x , 0x%x not the same\n",newBuffers->paddr,buf.m.userptr );
            buf.m.userptr  = newBuffers->paddr;
            //return -1;
        }
        
    } else if(m_mem_type == V4L2_MEMORY_MMAP) {   
        if (ioctl (mV4l2Fd, VIDIOC_DQBUF, &buf) < 0) {
            log_print(_ERROR,"VidDisp::display() line%d VIDIOC_QBUF failed: %s\n",__LINE__, strerror(errno));
            return -1;      
        }
        memcpy(buffers[buf.index].start,newBuffers->vaddr,buffers[buf.index].length);
    }
    
        
    //log_print(_INFO,"VidDisp::display() VIDIOC_QBUF index%d, sequence:%d timestamp sec%u usec:%u\n", buf.index, buf.sequence,buf.timestamp.tv_sec, buf.timestamp.tv_usec);
    if (ioctl (mV4l2Fd, VIDIOC_QBUF, &buf) < 0) {
        log_print(_ERROR,"VidDisp::display() %d line VIDIOC_QBUF failed\n",__LINE__);
        return -1;
    }

    return 0;
}

int VidDisp::startDisplay()
{
    int ret = 0;
    log_print(_DEBUG," VidDisp::startDisplay \n");
       int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    
    if(isStarted) {
        log_print(_ERROR,"VidDisp::startDisplay() stream has already started\n");
        return -1;
    }
    memset(&buf_req, 0, sizeof(buf_req));

    if(m_mem_type == V4L2_MEMORY_MMAP) {
        buf_req.count = DISP_BUFFER_NUM;
        m_num_buffers = buf_req.count;
    } else if(m_mem_type == V4L2_MEMORY_USERPTR) {
        buf_req.count = DISP_BUFFER_MAX_NUM;
        if(m_num_buffers == 0) {
            log_print(_ERROR,"VidDisp::startDisplay() start with no buffers \n");
            return -1;
        }
    }
    
    /*User buffers are allocated by applications themselves, and this ioctl is merely used to
    switch the driver into user pointer I/O mode and to setup some internal structures.*/
    buf_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buf_req.memory = m_mem_type;
    if (ioctl(mV4l2Fd, VIDIOC_REQBUFS, &buf_req) < 0) {
            log_print(_ERROR,"VidDisp::startDisplay() request buffers failed\n");
            return -1;
    }
    log_print(_INFO,"VidDisp::startDisplay() using  %d/%d buffers\n", m_num_buffers,DISP_BUFFER_MAX_NUM);


    for (int i = 0; i < m_num_buffers; i++) {
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf.memory = m_mem_type;
        buf.index = i;

        if (m_mem_type == V4L2_MEMORY_USERPTR) {
            buf.m.userptr = (unsigned long) buffers[i].offset;
            buf.length = buffers[i].length;
            log_print(_INFO,"VidDisp::startDisplay() buf.m.userptr 0x%x, buf.length %d\n",buf.m.userptr,buf.length);
        } else if(m_mem_type == V4L2_MEMORY_MMAP) {
            //This ioctl is part of the streaming I/O method.
            if (ioctl(mV4l2Fd, VIDIOC_QUERYBUF, &buf) < 0) {
                log_print(_ERROR,"VidDisp::startDisplay() VIDIOC_QUERYBUF error\n");
                    return -1;
            }
            buffers[i].length = buf.length;
            buffers[i].offset = (size_t) buf.m.offset;
            log_print(_DEBUG,"VIDIOC_QUERYBUF: length = %d, offset = %d\n",buffers[i].length, buffers[i].offset);
            
            buffers[i].start = NULL;
            buffers[i].start = (unsigned char *)mmap (NULL, buffers[i].length,
                                 PROT_READ | PROT_WRITE, MAP_SHARED,
                                 mV4l2Fd, buffers[i].offset);
            
            if (buffers[i].start == NULL) {
                log_print(_ERROR,"v4l2_out test: mmap failed\n");
                return -1;
            }
            memset(buffers[i].start, 0x00, buffers[i].length);
            if (ioctl(mV4l2Fd, VIDIOC_QUERYBUF, &buf) < 0) { //second times query for MMAP to get physical address, but USERPTR mode will cause kernel to die loop
                log_print(_ERROR,"VidDisp::startDisplay() VIDIOC_QUERYBUF error\n");
                return -1;
            }
            printf("VIDIOC_QUERYBUF: buf.m.offset = %x\n",buf.m.offset);
        }
    /*
        if ((ret = ioctl (mV4l2Fd, VIDIOC_QBUF, &buf)) < 0) {
                log_print(_INFO,"VidDisp::startDisplay() VIDIOC_QBUF error  %d\n",ret);
                perror ("VIDIOC_QBUF");
                return -1;
        }*/
    }
/*

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl (mV4l2Fd, VIDIOC_STREAMON, &type) < 0) {
        printf("VidDisp::startDisplay() Could not start stream\n");
        return -1;
    }*/
    //isStarted = true;
    //fb_setup(255,true,0x400);
    return 0;
}

int VidDisp::stopStoplay()
{
    log_print(_DEBUG," VidDisp::stopStoplay \n");
    if(!isStarted) {
    log_print(_ERROR,"VidDisp::stopStoplay() Not started\n");
    return -1;
    }
//printf(" ##########VidDisp::stopStoplay \n");

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    if (ioctl (mV4l2Fd, VIDIOC_STREAMOFF, &type) < 0) {
        log_print(_ERROR,"VidDisp::stopStoplay() Could not start stream\n");
        return -1;
    }
//printf(" ##########VidDisp::stopStoplay \n");
    if(m_mem_type == V4L2_MEMORY_MMAP) {
         for (int i = 0; i < m_num_buffers; i++) {
            munmap (buffers[i].start, buffers[i].length);
         }
 //  }
    //printf(" ##########VidDisp::stopStoplay \n");
        memset(&buf_req, 0, sizeof(buf_req));
        buf_req.count = 0;
        buf_req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        buf_req.memory = m_mem_type;
        int ret;
        if ((ret = ioctl(mV4l2Fd, VIDIOC_REQBUFS, &buf_req)) < 0) {
            log_print(_ERROR,"VidDisp::stopStoplay() free request buffers failed %d\n",ret);
            perror("VIDIOC_REQBUFS");
        }
        //printf(" ##########VidDisp::stopStoplay \n");
        m_num_buffers = 0;
    }

    isStarted = false;
    return 0;
}

int VidDisp::openDev()
{
    log_print(_DEBUG," VidDisp::openDev %s\n",devPath);
    if ((mV4l2Fd = open(devPath, O_RDWR|O_NONBLOCK)) < 0) { //must use O_NONBLOCK, or DQBUF may us 20~30ms 
        log_print(_ERROR,"VidDisp::openDev() Unable to open %s\n", (char*)devPath);
        return -1;
    }
    return 0;
}

int VidDisp::closeDev()
{
    log_print(_DEBUG," VidDisp::closeDev %s\n",devPath);
    close(mV4l2Fd);
    return 0;
}

int VidDisp::fb_setup(int alpha,bool colorkey_enable,u32 colorkey)
{
    int fd;
    int ret = 0;
    fd = open("/dev/fb0",O_RDWR);   

    struct mxcfb_gbl_alpha gbl_alpha;
    gbl_alpha.alpha= alpha;
    gbl_alpha.enable = 1;
    ret = ioctl(fd, MXCFB_SET_GBL_ALPHA, &gbl_alpha);
    if(ret <0) {
        printf("Error!VidDisp::MXCFB_SET_GBL_ALPHA failed for dev \n");
        return -1;
    }   

    if (colorkey_enable) {
        struct mxcfb_color_key key;
        key.enable = 1;
        key.color_key = colorkey; // Black
        ret = ioctl(fd, MXCFB_SET_CLR_KEY, &key);
        if(ret <0) {
            printf("Error!Colorkey setting failed for dev \n");
            return -1;
        }   
    }
    close(fd);
}

