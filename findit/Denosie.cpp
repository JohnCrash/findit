//
//  Denosie.cpp
//  findit
//
//  Created by zuzu on 13-3-15.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#include "Denosie.h"

namespace denosie
{
    struct scanpt
    {
        int x,y;
        int xend; //数量
        
        scanpt(int _x,int _y,int _xend):
            x(_x),y(_y),xend(_xend)
        {
        }
    };

    class cache
    {
    public:
        scanpt* alloc(int n)
        {
            if( c.empty() )
                return new scanpt(0,0,0);
            else
            {
                scanpt* p = c.back();
                c.pop_back();
                return p;
            }
        }
        void free(scanpt* p)
        {
            c.push_back(p);
        }
        cache()
        {
        }
        virtual ~cache()
        {
            for(std::vector<scanpt*>::iterator i=c.begin();i!=c.end();++i)
                delete *i;
            c.clear();
        }
    private:
        std::vector<scanpt*> c;
    };

    class creep
    {
    public:
        creep(uchar* _in,uchar* _out,int _rows,int _cols):
        rows(_rows),cols(_cols),din(_in),dout(_out)
        {
        }
        int seg_up_down(int x,int y,int xend)
        {
            int yy,start,end;
            if( y>0 )
            {
                yy = y-1;
                for(int i=x;i<xend;++i)
                { //发现和该段上下相连的那些段
                    if( seg_find(i,yy,start,end) )
                    {
                        seg_copy_zero(start,yy,end);
                        c->push_back(scanpt(start,yy,end));
                    }
                }
            }
            if( y<rows-1 )
            {
                yy = y+1;
                for(int i=x;i<xend;++i)
                { //发现和该段上下相连的那些段
                    if( seg_find(i,yy,start,end) )
                    {
                        seg_copy_zero(start,yy,end);
                        c->push_back(scanpt(start,yy,end));
                    }
                }
            }
            return xend-x;
        }
        void seg_copy_zero(int x,int y,int end)
        {
            uchar* p1 = din+y*cols;
            uchar* p2 = dout==0?0:dout+y*cols;
            for(int i=x;i<end;++i)
            {
                if( p2 != 0 )
                    *(p2+i) = *(p1+i); //copy
                *(p1+i) = 0; //zero
            }
        }
        //从一点开始查找头和尾,空的返回false
        bool seg_find(int x,int y,int& start,int& end)
        {//向后
            int i,j;
            uchar* p = din+y*cols;
            for(i=x;i<cols;++i)
            {
                if( *(p+i)==0 )
                    break;
            }
            for(j=x;j>=0;--j)
            {
                if( *(p+j)==0 )
                    break;
            }
            start = j+1;
            end = i;
            return end-start>0;
        }
        void swap()
        { //交换a,b,c
            if( c == &a )
            {
                c = &b;
                d = &a;
            }
            else
            {
                c = &a;
                d = &b;
            }
            c->clear(); //对当前的作清空处理
        }
        /*
         沿着起点x,y开始在in中蔓延爬行
         如果bcopy是true把蔓延到的点复制到out中去,同时从in中删除
         bcopy是false仅仅从out中删除,不进行复制
         返回有多少点被转移
         */
        int go(int x,int y,bool bcopy)
        {
            
            int start,end;
            int count = 0;
            uchar* din_tmp;
            if( !bcopy )
            {//交换in,out
                din_tmp = din;
                din = dout;
                dout = 0;
            }
            
            swap();
            if(!seg_find(x,y,start,end))return count;
            seg_copy_zero(start,y,end);
            count += seg_up_down(start, y, end);
            swap();
            do
            {
                for(std::vector<scanpt>::iterator i=d->begin();i!=d->end();++i)
                {
                    count += seg_up_down(i->x,i->y,i->xend);
                }
                swap();
            }while(!d->empty());
            
            if( !bcopy )
            {//交换in,out
                dout = din;
                din = din_tmp;
            }
            return count;
        }
    private:
        int rows;
        int cols;
        uchar* din;
        uchar* dout;
        std::vector<scanpt> a;
        std::vector<scanpt> b;
        std::vector<scanpt>* c;
        std::vector<scanpt>* d;
    };
};
/*
    对CV_8UC1图进行降噪处理,in输入图,out输出,ns噪音点数
    小于ns的连通块都被删除
    因为随着爬行进行,白点越来越少.因此该算法并不慢
 */
void deNosie(cv::Mat& in,cv::Mat& out,int ns)
{
    int rows,cols;
    
    rows = in.rows;
    cols = in.cols;
    
    assert(in.type()==CV_8UC1);
    if( out.empty() )
        out.create(rows,cols,in.type());
    //out.zeros(rows, cols, in.type());
    //在out是一个SubMat时,zeros不能正常工作.
    for( int i = 0;i < rows;++i )
    {
        uchar* p = out.ptr(i);
        for( int j = 0;j< cols;++j )
            *(p+j) = 0;
    }
    
    denosie::creep walker(in.data,out.data,rows,cols);
    
    for( int y=0;y<rows;++y )
    {
        uchar* cur = in.ptr(y);
        for( int x=0;x<cols;++x )
        {
            if( *(cur+x)==255 )
            {
                if( walker.go(x,y,true) <= ns )
                { //发现噪点
                    walker.go(x,y,false);//在out中删除噪点
                }
            }
        }
    }
}
/*
    多核版本deNosie
    简单的横向分割输入图像,然后并行做去除噪点的操作
 */
void deNosie_OMP(cv::Mat& in,cv::Mat& out,int ns)
{
    int rows,cols,rows4;
    
    rows = in.rows;
    cols = in.cols;
    rows4 = rows/4;
    assert(in.type()==CV_8UC1);
    
    out.create(rows,cols,in.type());
    out.zeros(rows, cols, in.type());
    //分成四块
    int k;
#pragma omp parallel for private(k)
    for( k=0;k<4;++k ){
        denosie::creep walker(in.ptr(k*rows4),out.ptr(k*rows4),rows4,cols);
        for( int y=0;y<rows4;++y )
        {
            uchar* cur = in.ptr(y+k*rows4);
            for( int x=0;x<cols;++x )
            {
                if( *(cur+x)==255 )
                {
                    if( walker.go(x,y,true) <= ns )
                    { //发现噪点
                        walker.go(x,y,false);//在out中删除噪点
                    }
                }
            }
        }
    }
}