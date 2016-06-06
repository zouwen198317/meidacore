CXX=arm-poky-linux-gnueabi-g++

LIBS_PATH=-L../../../work/fsl-release-bsp/build/tmp/work/imx6qsabresd-poky-linux-gnueabi/imx-lib/1_3.10.17-1.0.0-r0/image/usr/lib/libipu.so

INCPATH=-I../../../work/fsl-release-bsp/build/tmp/work/imx6qsabresd-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include/uapi -I../../../work/fsl-release-bsp/build/tmp/work/imx6qsabresd-poky-linux-gnueabi/linux-imx/3.10.17-r0/git/include



iputest:main.o imageprocessunit.o imageprocess.o
	$(CXX) -o iputest main.o imageprocessunit.o imageprocess.o $(LIBS_PATH) $(INCPATH)

.c.o:
	$(CXX) -c $(LIBS_PATH) $(INCPATH) -o $@ $< 
.cpp.o:
	$(CXX) -c $(LIBS_PATH) $(INCPATH) -o $@ $< 
all:iputest
	
.PHONY:clean
clean:
	rm -f iputest main.o imageprocessunit.o
	
	
