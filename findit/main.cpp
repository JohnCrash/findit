//
//  main.cpp
//  findit
//
//  Created by zuzu on 13-3-7.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//
#include <iostream>
#include "RgnGrid.h"
#include "Denosie.h"

static int thre=128;
int block_size = 11;
int param = 5;
Mat src,dst,dstb,deno;

static void update(int,void*)
{
  //  threshold(dst,dstb,thre,255,THRESH_BINARY);
    //ADAPTIVE_THRESH_MEAN_C
    //ADAPTIVE_THRESH_GAUSSIAN_C
    if( block_size%2!=1)
        block_size++;
    if(block_size<=2)
        block_size = 3;
    adaptiveThreshold(dst,dstb,255,ADAPTIVE_THRESH_MEAN_C,CV_THRESH_BINARY_INV,block_size,param);
        
    deNosie(dstb, deno,9);
    RgnGrid grid(deno.cols,deno.rows,deno.data);
    grid.drawTL(src);
    grid.End();
    /*
    int cols,rows;
    cols = src.cols;
    rows = src.rows;
    int tt = thre;
    for( int i = 0;i < cols;++i )
    {
        uchar* p = src.data + i*rows*3;
        uchar* o = dst.data + i*rows;
        for( int j = 0;j < rows;++j )
        {
            uchar r,g,b;
            uchar* c = p+3*j;
            b = *(c);
            g = *(c+1);
            r = *(c+2);
            int v = max(b,max(r,g));
            if( v < tt )
                *(o+j) = 0;
            else
                *(o+j) = 255;
        }
    }*/
    imshow("denosie", deno);
    imshow("source", src);
}

Mat getMat()
{
    Mat m(16,16,CV_64FC4);
    m.data[0] = 11;
    return m;
}
int main(int argc, const char * argv[])
{

    const char* filename = argc >= 2 ? argv[1] : "/Users/zuzu/Source/opencv/wq/image/6.jpg";
    src = imread(filename, 1);
    if(src.empty())
    {
        cout << "can not open " << filename << endl;
        return -1;
    }
    cvtColor(src, dst, CV_BGR2GRAY,1);
    imshow("source", src);

  //  createTrackbar("thre","source",&thre,255,update);
  //  createTrackbar("block","source",&block_size,255,update);
  //  createTrackbar("param","source",&param,255,update);
    update(0,0);
    waitKey();
    return 0;
}

