#include<iostream>
#include"string_overlay_gray.h"
#include<algorithm>
#include<iterator>
#include<string>
#include<fstream>
using namespace std;

int main()
{
	ifstream in("gray.raw",ios::binary);
	ofstream out("out.raw",ios::binary);

	uchar grayBuf[640*480];
	copy(istream_iterator<uchar>(in),istream_iterator<uchar>(),grayBuf);

	string str;
	getline(cin,str);

	font_info f_info;
	f_info.width = 16;
	f_info.height = 32;
	f_info.str = str;
	f_info.path = "./dotfont/ASC32";
	f_info.background = rgb(0,67,252);
	f_info.font_color = rgb(255,128,64);

	uchar ** dotBuf = new uchar*[f_info.str.size()];
	uint font_bytes = f_info.width/8*f_info.height;
	for(int i=0;i < f_info.str.size();++i)
		dotBuf[i] = new uchar[font_bytes];

	string_to_dot_matrix(f_info.path.data(),font_bytes,f_info.str.data(),dotBuf);

	dot_overlay_gray(dotBuf,f_info,grayBuf,640,480,600,100);
	dot_overlay_gray(dotBuf,f_info,grayBuf,640,480,100,100);
	dot_overlay_gray(dotBuf,f_info,grayBuf,640,480,100,460);
	dot_overlay_gray(dotBuf,f_info,grayBuf,640,480,600,460);

	copy(grayBuf,grayBuf+640*480,ostream_iterator<uchar>(out));
	
	in.close();
	out.close();

	for(int i = 0;i < f_info.str.size();++i)
			delete [] dotBuf[i];
	delete [] dotBuf;


}
