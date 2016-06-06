#include "muxer.h"

Muxer::Muxer(char *name, muxParms *pParm)
{

}

Muxer::~Muxer()
{

}

int Muxer::write(AVPacket *pkt)
{
	return -1;
}

int Muxer::read(AVPacket *pkt)
{
	return -1;
}

int Muxer::writeVideoPkt(AVPacket *pkt)
{
	return -1;
}

int Muxer::writeAudioPkt(AVPacket *pkt)
{
	return -1;
}

int Muxer::writeDataPkt(AVPacket *pkt)
{
	return -1;
}
/*
AlgChk* Muxer::getFcwChkHead()
{
	return NULL;
}

AlgChk* Muxer::getLdwChkHead()
{
	return NULL;
}

AlgChk* Muxer::getMdwChkHead()
{
	return NULL;
}
*/
