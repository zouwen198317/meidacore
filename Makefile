WORK_PATH = $(PWD)

export WORK_PATH
#CROSS_COMPILE =arm-hismall-linux-
CROSS_COMPILE =arm-poky-linux-gnueabi-
LIBS_SHARE    = -L../common/ -lcommon -lasound -lpthread -lrt -lssl -lcrypto -ldl -lxml2

		
PLATFORMINC=/work/fsl-release-bsp/build-ai-x11/tmp/sysroots/imx6qsabreauto/usr/include
XMLINC=$(PLATFORMINC)/libxml2 
PLATFORMLIB=/work/fsl-release-bsp/build-ai-x11/tmp/sysroots/imx6qsabreauto/usr/lib

LIBS_SHARE    = -L../common/ -lcommon -L./platform/IMX6/ipu_lib -L./platform/IMX6/g2d_lib -lipu  -lasound -lpthread -lrt -lssl -lcrypto -ldl -lxml2 #-lg2d


				
LIBS_PATH = 		
LIBS	= ./platform/IMX6/vpu_lib/libvpu.a ../license/liblicense.a
			

INCPATH = -I$(WORK_PATH) -I../license -I../common -I./include -I./libthread -I./platform -I../common/ringbuffer -I./mediafile \
		-I./mediafile/ffmpeg -I./mediafile/ffmpeg/avformat -I./mediafile/ffmpeg/audiocodec -I./mediafile/flv -I./mediafile/avi \
		-I../include -I../ini -I./platform/IMX6 \
-I/work/fsl-release-bsp/build-ai-x11/tmp/work/imx6qsabreauto-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include/uapi \
		-I/work/fsl-release-bsp/build-ai-x11/tmp/work/imx6qsabreauto-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include

CXX	= $(CROSS_COMPILE)g++
CC	= $(CROSS_COMPILE)gcc
LINK =  $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip
CXXFLAGS= -Wall -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DIMX6_PLATFORM  -g -O2 -DUSE_SDCARD -DVERIFY_LICENSE #-DNO_CAM #-DDEBUG_EMC #-DNO_CAM
TARGET	= mediacore

####### Dirs
#OBJDIRS += 	xxx


OBJDIRS-clean := $(foreach obj,$(OBJDIRS),$(obj)-clean)

####### Files

#SRC += mediacore.c

OBJS +=mediacore.o
OBJS +=readPacketjob.o
#OBJS +=readPacketjob_net.o
OBJS +=imageprocessjob.o
OBJS +=videoprepencodejob.o
OBJS +=videocapturejob.o
OBJS +=videoencodejob.o
OBJS +=recfilejob.o
OBJS +=overridejob.o
OBJS +=opccommand.o
OBJS +=playbackjob.o
OBJS +=audiocapturejob.o
OBJS +=alg.o
OBJS +=AviAMFData.o
OBJS +=exceptionProcessor.o
OBJS +=motion_detection_technology.o

OBJS +=libthread/threadpp.o

OBJS +=platform/camcapture.o
OBJS +=platform/microphone.o
OBJS +=platform/videodisp.o
OBJS +=platform/audioplay.o
OBJS +=platform/HAL/imageprocess.o
OBJS +=platform/IMX6/imageprocessunit.o
#OBJS +=platform/IMX6/gpu2dblit.o
OBJS +=platform/IMX6/vpuenc.o
OBJS +=platform/IMX6/vpudec.o
OBJS +=platform/IMX6/framebuf.o

OBJS +=platform/HAL/videoencoder.o
OBJS +=platform/swcodec/adpcmencoder.o
OBJS +=platform/swcodec/adpcmdecoder.o

#OBJS +=ringbuffer/sharememory.o

OBJS +=../ini/parms_dump.o
OBJS +=../ini/config_reader.o

#ffmpeg avformat
OBJS +=mediafile/ffmpeg/avformat/avc.o
OBJS +=mediafile/ffmpeg/avformat/avi.o
OBJS +=mediafile/ffmpeg/avformat/avidec.o
OBJS +=mediafile/ffmpeg/avformat/avienc.o
OBJS +=mediafile/ffmpeg/avformat/avio.o
OBJS +=mediafile/ffmpeg/avformat/aviobuf.o
OBJS +=mediafile/ffmpeg/avformat/file.o
OBJS +=mediafile/ffmpeg/avformat/flvdec.o
OBJS +=mediafile/ffmpeg/avformat/flvenc.o
OBJS +=mediafile/ffmpeg/avformat/intfloat_readwrite.o
OBJS +=mediafile/ffmpeg/avformat/mathematics.o
OBJS +=mediafile/ffmpeg/avformat/metadata.o
OBJS +=mediafile/ffmpeg/avformat/msvdec.o
OBJS +=mediafile/ffmpeg/avformat/msvenc.o
OBJS +=mediafile/ffmpeg/avformat/rational.o
OBJS +=mediafile/ffmpeg/avformat/riff.o
OBJS +=mediafile/ffmpeg/avformat/utils.o

OBJS +=mediafile/ffmpeg/audiocodec/adpcm.o
OBJS +=mediafile/ffmpeg/audiocodec/audioconvert.o
OBJS +=mediafile/ffmpeg/audiocodec/mem_t.o
OBJS +=mediafile/ffmpeg/audiocodec/resample.o
OBJS +=mediafile/ffmpeg/audiocodec/resample2.o

OBJS +=mediafile/avimuxer.o
OBJS +=mediafile/avidemuxer.o
OBJS +=mediafile/jpgfile.o
OBJS +=mediafile/muxer.o
OBJS +=mediafile/rawfile.o
OBJS +=mediafile/bmpfile.o

OBJS +=overlayer/createSingleColorBg.o
OBJS +=overlayer/dot_rgb.o
OBJS +=overlayer/string_overlay_gray.o
OBJS +=overlayer/drawGraph_I420.o
OBJS +=overlayer/drawGraph_yuyv.o
OBJS +=overlayer/string_overlay_yuyv.o
OBJS +=overlayer/string_overlay_I420.o
####### Implicit rules

.SUFFIXES: .cpp .cxx .cc .C .c

.cpp.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

.cxx.o:
	$(CXX) -c $(CXXFLAGS) $(INCPATH) -o $@ $<
	
.c.o:
	$(CC) -c $(CXXFLAGS) $(INCPATH) -o $@ $<

####### Build rules

#-rdynamic
all: $(TARGET)

$(TARGET):$(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(LIBS_SHARE) $(LIBS) $(INCPATH) $(CXXFLAGS)
	#$(STRIP) $(TARGET)

$(OBJDIRS) $(OBJDIRS-clean): dummy

#
## Generic rules
#

%:
	[ ! -d $* ] || $(MAKE) -C $*

%-clean:
	[ ! -d $* ] || $(MAKE) -C $* clean


clean:	$(OBJDIRS-clean)
	-rm -f $(OBJS) $(TARGET)

.PHONY:dummy
