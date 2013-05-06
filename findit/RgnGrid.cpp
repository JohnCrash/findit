//
//  RgnGrid.cpp
//  findit
//
//  Created by john on 13-3-9.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//
#include "RgnGrid.h"
#include "Denosie.h"
#include <fstream>

//测试性能用,一个作用域计时器.打印析构时间减去构造事件
class scope_timer
{
public:
    scope_timer( string _n )
    {
        t = boost::posix_time::microsec_clock::local_time();
        last = t;
        name = _n;
    }
    void escaped( string msg )
    {
        boost::posix_time::time_duration d = boost::posix_time::microsec_clock::local_time() - last;
        last = boost::posix_time::microsec_clock::local_time();
        cout << "'" << msg <<"' duration: " << d.total_microseconds()/1000.0 << " ms" << endl;
    }
    ~scope_timer()
    {
        boost::posix_time::time_duration d = boost::posix_time::microsec_clock::local_time() - t;
        cout << "'" << name  <<"' tatol duration: " << d.total_microseconds()/1000.0 << " ms" << endl;
    }
private:
    boost::posix_time::ptime t;
    boost::posix_time::ptime last;
    string name;
};
//测试用，保存直线到svg格式
void save(vector<Lines>& lines)
{
    std::ofstream f("/Users/zuzu/Desktop/3.svg");
    int id = 2000;
    for(vector<Lines>::iterator it = lines.begin();it!=lines.end();++it)
    {
        f << "<path style=\"fill:none;stroke:#000000;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1\"\n";
        f << "d = \"m "<<it->points[0]<<","<<it->points[1]<<","<<it->points[2]-it->points[0]<<","<<it->points[3]-it->points[1]<<"\"\n";
        f << "id=\"path" << id++ << "\"\n";
        f << "inkscape:connector-curvature=\"0\" />\n";
    }
}
//使用两点构造直线L
static void buildLine(int p1[2],int p2[2],Vec4f& L)
{
    L[0] = p2[0]-p1[0];
    L[1] = p2[1]-p1[1];
    L[2] = p1[0];
    L[3] = p1[1];
}
//计算两条直线的交点
static Point2f Cross(Vec4f& l0,Vec4f& l1)
{
    Point2f cp;
    float t0 = (l1[2]-l0[2])*l1[1]+(l0[3]-l1[3])*l1[0];
    float r = l1[1]*l0[0]-l1[0]*l0[1];
    if( r == 0 )
        t0 = FLT_MAX;
    else
        t0 /= r;
    cp.x = l0[0]*t0+l0[2];
    cp.y = l0[1]*t0+l0[3];
    return cp;
}

/*
    RgnGrid可以复用
    创建的时候自动创建多个线程,节省使用中每次创建线程的时间花费
 */
RgnGrid::RgnGrid()
{
    mBarrier = NULL;
    resetThread(true);
}

//重新创建线程b,指出是否分配和核心数相同的线程.
void RgnGrid::resetThread(bool b)
{
    releaseThread();
    int cores = boost::thread::hardware_concurrency();
    
    if( cores <= 0 )
    {
        cout<<"error hardware_concurrency() <= 0!";
        cores = 1;
    }
    mIsMutiCores = b;
    
    mExit = false;
    
    mMainLoopGo = false;
    if( b )
        mBarrier = new boost::barrier(cores);
    else
        mBarrier = NULL;

    mThread.push_back(new boost::thread(boost::bind(&RgnGrid::mainRgn,this)));
    if( b )
    {
        for( int i=1;i<cores;++i )
        {
            mThread.push_back(new boost::thread(&RgnGrid::threadRgn,this,i));
        }
    }
}
//iswait = true 将等待处理完返回,否则将立刻返回
void RgnGrid::rgnM( Mat& m ,bool iswait,ProgressFunc progress )
{
    mProgressFunc = progress;
    mSrc = m;
    {
        boost::mutex::scoped_lock lock(mMu);
        mMainLoopGo = true;
    }
    mMainLoopComplate = false;
    mCondition.notify_one(); //启动主处理循环
    if( iswait )
    {
        boost::mutex::scoped_lock lock(mMu2);
        while( !mMainLoopComplate )
        {
            mCondition2.wait(mMu2);
        }
    }
}

Mat RgnGrid::getBinaryMat()
{
    return mB2;
}

Mat RgnGrid::getSourceMat()
{
    return mSrc;
}

//终止线程
void RgnGrid::releaseThread()
{
    mExit = true;
    {
        boost::mutex::scoped_lock lock(mMu);
        mMainLoopGo = true;
    }
    mCondition.notify_one();
    
    for( int i = 0;i < mThread.size();++i )
    {
        mThread[i]->join();
        delete mThread[i];
    }
    delete mBarrier;
    mThread.clear();
}

void RgnGrid::mainRgn()
{
    while(!mExit)
    {
        {
            boost::mutex::scoped_lock lock(mMu);
            while( !mMainLoopGo )
            {
                mCondition.wait(mMu);
            }
        }
        if( mExit )
        {
            if(mBarrier)mBarrier->wait();
            return;
        }
        //分配任务
        if( !mSrc.empty() )
        {
            scope_timer st("main");
            clear(); //清楚状态
            int cores = (int)mThread.size();
            cout << "cores: " << cores << endl;
            mB2.create(mSrc.rows, mSrc.cols, CV_8UC1);
            mDes.create(mSrc.rows, mSrc.cols, CV_8UC1);
            rows = mSrc.rows;
            cols = mSrc.cols;
            data = mB2.data;
            int h = mSrc.rows/cores;
            for( int i = 0; i < cores;++i )
            {
                mSrcs.push_back(mSrc.rowRange(i*h,(i+1)*h));
                mB2s.push_back(mB2.rowRange(i*h,(i+1)*h));
                mDess.push_back(mDes.rowRange(i*h,(i+1)*h));
            }
            if( mBarrier )mBarrier->wait(); //启动
            cvtColor(mSrcs[0],mB2s[0],CV_RGBA2GRAY,1);
            if( mBarrier )mBarrier->wait(); //1
            adaptiveThreshold(mB2,mDes,255,ADAPTIVE_THRESH_MEAN_C,CV_THRESH_BINARY_INV,11,5);
            if( mBarrier )mBarrier->wait(); //2
            deNosie(mDess[0], mB2s[0], 9);
            if( mBarrier )mBarrier->wait(); //3
            mProgressFunc(0.1); //开始部分算10%
            st.escaped("binard denosie");
            mDess.clear();
            mB2s.clear();
            mSrcs.clear();
            mDes.release(); //释放中间数据
            /*
             准备做块处理,使得Rgn()可以并行处理数据
             */
            mProgress = 0.1;
            praperRgnMultiThreadProcess();
            mDeltaProgress = 0.8/mRgnOffsetPts.size();
            if( mBarrier )mBarrier->wait(); //4
            Point2i pt;
            while( getOffsetPt( pt ) )
            {
                threadRgnExpand( pt.x,pt.y );
                mProgress += mDeltaProgress;
                mProgressFunc(mProgress);
            }
            if( mBarrier )mBarrier->wait(); //5
            mProgressFunc(0.9); //块部分80%
            st.escaped("block process");
            RgnEdge();
            GuessGrid();
            st.escaped("edge guess");
            mProgressFunc(1.0); //最后算10%
        }
        //通知完成任务
        {
            boost::mutex::scoped_lock lock(mMu2);
            mMainLoopComplate = true;
        }
        mCondition2.notify_one();
        mMainLoopGo = false;
    }
}

void RgnGrid::threadRgn( int i )
{
    while(!mExit)
    {
        mBarrier->wait(); //启动点
        if( mExit )return;
        cvtColor(mSrcs[i],mB2s[i],CV_RGBA2GRAY,1);
        mBarrier->wait(); //1
        mBarrier->wait(); //2
        deNosie(mDess[i], mB2s[i], 9);
        mBarrier->wait(); //3
        mBarrier->wait(); //4
        Point2i pt;
        while( getOffsetPt( pt ) )
        {
            threadRgnExpand( pt.x,pt.y );
            mProgress += mDeltaProgress;
            mProgressFunc(mProgress);
        }
        mBarrier->wait(); //5
    }
}

void RgnGrid::threadRgnExpand( int offx,int offy )
{
    for( int y = offy;y<mThread_h;y+=mThread_BlockSize )
    {
        for( int x = offx;x<mThread_w;x+=mThread_BlockSize )
        {
            RgnExpand(x, y, mThread_BlockSize);
        }
    }
}

bool RgnGrid::getOffsetPt( Point2i& p )
{
    boost::mutex::scoped_lock lock(mMutex);
    if( mOffsetPtsCount == 0 )
        return false;
    p = mRgnOffsetPts[--mOffsetPtsCount];
    return true;
}

//准备多线程处理数据,好让多个线程同时处理
void RgnGrid::praperRgnMultiThreadProcess()
{
    int w,h,offset,offset_step;
    /*将数据图像切分成很多小块,根据经验块的大小为较短边长的1%~2%
     然后对这些块便宜
     */
    int block_size = min(cols,rows)/50 + 1;
    search_block_size = block_size;
    helf_block_size = block_size/2;
    w = cols-block_size;
    h = rows-block_size;
    offset = block_size/3;
    offset_step = offset%2==0?offset:offset+1;
    
    mRgnOffsetPts.clear();
    for( int offy = 0;offy<block_size;offy+=offset_step )
    {
        for( int offx = 0;offx<block_size;offx+=offset_step )
        {
            mRgnOffsetPts.push_back( Point2i(offx,offy) );
        }
    }
    mOffsetPtsCount = (int)mRgnOffsetPts.size();
    mThread_BlockSize = block_size;
    mThread_w = w;
    mThread_h = h;
}

RgnGrid::~RgnGrid()
{
    releaseThread();
    destoryAllBlock();
}

void RgnGrid::drawTLPt(Mat& mt,const TLPt& pt )
{
    int x1,y1;
    Scalar color;
    if( pt.m == 1)
        color = Scalar(128,255,0);
    else if( pt.m ==2 )
        color = Scalar(0,128,255);
    else if( pt.m == 3 )
        color = Scalar(128,0,255);
    else
        color = Scalar(255,0,128);
    x1 = pt.line[0]*30+pt.ox;
    y1 = pt.line[1]*30+pt.oy;
    line(mt,Point(pt.ox,pt.oy),Point(x1,y1),Scalar(0,0,255),1,CV_AA);
    x1 = pt.line[2]*30+pt.ox;
    y1 = pt.line[3]*30+pt.oy;
    line(mt,Point(pt.ox,pt.oy),Point(x1,y1),Scalar(255,0,0),1,CV_AA);
    rectangle(mt,Point(pt.ox-1,pt.oy-1),Point(pt.ox+1,pt.oy+1),Scalar(0,255,0),2,8,0);
    if( pt.ox == 640 && pt.oy == 955 )
        color = Scalar(255,255,255);
    if( pt.type==TTyle )
        rectangle(mt,Point(pt.x,pt.y),
                  Point(pt.x+pt.block_size,pt.y+pt.block_size),
                  color,2,8,0);
    else if( pt.type==LTyle)
        circle(mt,Point(pt.ox,pt.oy),pt.block_size,color,3,8,0);
    else if( pt.type==CTyle)
    {
        circle(mt,Point(pt.ox,pt.oy),5,color,3,8,0);
    }
}

static void draw_v4f(Mat& mt,Vec4f v4f,Scalar color)
{
    int x0,y0,x1,y1;
    float t = -1000;
    x0 = v4f[0]*t+v4f[2];
    y0 = v4f[1]*t+v4f[3];
    t = 1000;
    x1 = v4f[0]*t+v4f[2];
    y1 = v4f[1]*t+v4f[3];
    
    line(mt,Point(x0,y0),Point(x1,y1),color,2,CV_AA);
}
void RgnGrid::drawTL(Mat& mt,int type)
{
    Mat b(rows,cols,CV_8UC1,data);
  //  Rgn();
    
     //绘制TLs
    if( type & 1 )
    {
        for(TLVector::iterator i = TLs.begin();i!=TLs.end();++i)
        {
            Scalar color;
            int w = 1;
            if( i->type == TLType::TTyle )
            {
                if( i->m == 1)
                    color = Scalar(128,255,0);
                else if( i->m ==2 )
                    color = Scalar(0,128,255);
                else if( i->m == 3 )
                    color = Scalar(128,0,255);
                else
                    color = Scalar(0,0,0);
            }
            else
            {
                w = 4;
                if( i->m == 1)
                    color = Scalar(255,0,0);
                else if( i->m ==2 )
                    color = Scalar(0,0,255);
                else if( i->m == 3 )
                    color = Scalar(0,255,0);
                else
                    color = Scalar(255,255,0);
            }
            drawTLPt(mt, *i);
        }
    }
    //绘制中间数据
    if( type & 2 )
    {
        for( int k=0;k<4;k++ )
        {   Scalar color;
            if( k == 0)
                color = Scalar(255,0,0);
            else if( k ==1 )
                color = Scalar(0,0,255);
            else if( k == 2 )
                color = Scalar(0,255,0);
            else
                color = Scalar(255,255,0);
            for(TLVector::iterator i=TBorder[k].begin();i!=TBorder[k].end();++i )
            {
                drawTLPt(mt,*i);
            }
            drawTLPt(mt,Corner[k]);
            draw_v4f(mt,OuterBorder[k],color);
        }
     
        for( int k=0;k<4;k++ )
        {
            for(vector<Point2f>::iterator i=Edge[k].begin();i!=Edge[k].end();++i)
            {
                circle(mt, *i, 2, Scalar(255,0,0),2);
            }
        }
        for(TLVector::iterator i=CrossPt.begin();i!=CrossPt.end();++i)
        {
            drawTLPt(mt, *i);
        }
    }
    //绘制19x19网格
    if( type & 4 )
    {
        for(int i=0;i<19;++i)
        {
            if( i<Edge[0].size() && 18-i<Edge[2].size() )
                line(mt,Edge[0][i],Edge[2][18-i],Scalar(255,0,0),2,CV_AA);
            if( i<Edge[1].size() && 18-i<Edge[3].size() )
                line(mt,Edge[1][i],Edge[3][18-i],Scalar(0,0,255),2,CV_AA);
        }
    }
    //仅仅绘制T点
    if( type & 8 )
    {
        for( int k=0;k<4;k++ )
        {
            Scalar color;
            if( k == 0)
                color = Scalar(255,0,0);
            else if( k ==1 )
                color = Scalar(0,0,255);
            else if( k == 2 )
                color = Scalar(0,255,0);
            else
                color = Scalar(255,255,0);
            for(int i=0;i<TBorders[k].size();++i)
            {
                Vec2i v2i;
                vector<Vec2i> vp;

                for(int j=0;j<TBorders[k].at(i).size();++j)
                {
                    drawTLPt(mt,TBorders[k].at(i).at(j));
                    v2i[0] = TBorders[k].at(i).at(j).ox;
                    v2i[1] = TBorders[k].at(i).at(j).oy;
                    vp.push_back(v2i);
                }
                Vec4f v4f;
                if( vp.size()>1 )
                {
                    fitLine(vp,v4f,CV_DIST_L1,0,0,0);
                    draw_v4f(mt,v4f,color);
                }
                vp.clear();
            }
          //  for(TLVector::iterator i=TBorder[k].begin();i!=TBorder[k].end();++i )
          //  {
          //      drawTLPt(mt,*i);
          //  }
        }
    }
}

/*
 识别棋盘中的T和L型特征
 */
void RgnGrid::Rgn()
{
    int w,h,offset,offset_step;
    /*将数据图像切分成很多小块,根据经验块的大小为较短边长的1%~2%
     然后对这些块便宜
     */
    int block_size = min(cols,rows)/50 + 1;
    search_block_size = block_size;
    helf_block_size = block_size/2;
    w = cols-block_size;
    h = rows-block_size;
    offset = block_size/3;
    offset_step = offset%2==0?offset:offset+1;
    
    for( int offy = 0;offy<block_size;offy+=offset_step )
    {
        for( int offx = 0;offx<block_size;offx+=offset_step )
        {
            for( int y = offy;y<h;y+=block_size )
            {
                for( int x = offx;x<w;x+=block_size )
                {
                 //   uchar* p = data+y*cols+x;
                 //   TLType type = RngTL(p,x,y,block_size);
                 //   if( type != TLType::TNothing )
                 //   {
                 //       TLs.push_back(TLPt(type,x,y,block_size));
                 //   }
                    RgnExpand(x, y, block_size);
                }
            }
        }
    }
    
    destoryAllBlock();
}

void RgnGrid::RgnEdge()
{
    SelectMatch(5*CV_PI/180);
    SelectMatch2(5*CV_PI/180);
}

void RgnGrid::GuessGrid()
{
    if( Guess() )
    {
        printf("Found!\n");
    }
    else
    {
        printf("Error!\n");
    }
}

void RgnGrid::clear()
{
    TLs.clear();
    for(int i=0;i<4;++i)
    {
        TBorders[i].clear();
        TBorder[i].clear();
        Edge[i].clear();
        Corner[i].type = TNothing; //清除角点
        Intact[i] = 0;
    }
    LLPts.clear();
    LL[0].clear();
    LL[1].clear();
    CrossPt.clear();
    destoryAllBlock();
}
/* 扩大块尺寸直到不符合条件为止
 */
void RgnGrid::RgnExpand(int x,int y,int block_size)
{
    TLType type,last_type = TNothing;
    int xx,yy,last_size,last_x,last_y;
    uchar* p;
    type = TNothing;
    last_x = 0;
    last_y = 0;
    last_size = 0;
    
    for( int i=0;i<block_size;++i )
    {
        xx = x-i;
        yy = y-i;
        if( xx < 0 || yy < 0 || xx >= cols || yy >= rows )return;
        p = data+yy*cols+xx;
        type = RngTL( p,x,y,block_size+2*i );
        if( type == TNothing )
        {
            if( i == 0 )return;
            if( isInTLs(x,y,block_size) )//已经在里面了
                return;
            AddTLPt(last_type, last_x, last_y, last_size);
            return;
        }
        if( last_type!=TNothing && last_type!=type )
            return;
        last_type = type;
        last_size = block_size+2*i;
        last_x = xx;
        last_y = yy;
    }
    if( last_type != TNothing )
    {
        if( isInTLs(x,y,block_size) )//已经在里面了
            return;
        AddTLPt(last_type, last_x, last_y, last_size);
    }
}
//块已经在列表里面了返回true,否则返回false;
bool RgnGrid::isInTLs(int x,int y,int size)
{
    for( TLVector::iterator i =TLs.begin();i!=TLs.end();++i )
    {
        if( x >= i->x && x < i->x+i->block_size &&
            x+size >= i->x && x+size < i->x+i->block_size &&
           y >= i->y && y < i->y+i->block_size &&
           y+size >= i->y && y+size<i->y+i->block_size )
        {
            return true;
        }
    }
    return false;
}
//计算两个直线的交点,t0是交点在直线l0上的参数。
static bool Cross(Lines& l0,Lines& l1,float& t0,float& t1)
{
    float d,s;
    d = l0.cosa*l1.sina-l0.sina*l1.cosa;
    s = l0.sina*l1.sina+l0.cosa*l1.cosa;
    //分母d1 = -d;
    if( d == 0 )return false;
    
    t0 = (l0.distance*s-l1.distance)/d;
    t1 = -(l1.distance*s-l0.distance)/d;
    return true;
}

static float distance2(Point2f a,Point2f b)
{
    return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y));
}
//计算直线的sin,cos
static void calcLine(int p0[2],int p1[2],float l[2])
{
    float r = sqrt((p1[0]-p0[0])*(p1[0]-p0[0])+(p1[1]-p0[1])*(p1[1]-p0[1]));
    l[0] = (p1[0]-p0[0])/r;//cosa
    l[1] = (p1[1]-p0[1])/r;//sina
}
//返回值在-pi/2 pi/2之间
static float my_atan(float y,float x)
{
    float v = atan2f(y,x);
    if( v > CV_PI/2 )
        return v-CV_PI;
    if( v < -CV_PI/2 )
        return v+CV_PI;
    return v;
}
/*取得L,T型的交叉点.然后放入列表
 */
void RgnGrid::AddTLPt(TLType type,int x,int y,int size )
{
    int m[4];
    int ptt[4][2];
    int pt[4];
    bool b = false;
    uchar* p = data+y*cols+x;
    uchar* bk = getFreeBlock();
    copyBlockFromMain( p, bk, size);
    
    m[0] = LineMode(p,0,true,size);
    m[1] = LineMode(p,size-1,true,size);
    m[2] = LineMode(p,0,false,size);
    m[3] = LineMode(p,size-1,false,size);
    
    if( type == LTyle )
    {
        if( getLPt(bk,ptt,m,size) == 2 )
        {
           // printf("L:\n");
            if( getLVertex(bk, size, ptt[0], ptt[1], pt) )
            {
                b=true;
            }
        }
    }
    else if( type==TTyle )
    {
        if( getLPt(bk, ptt, m, size) == 4 )
        {
         //   printf("T:\n");
            //对T要做些修正,首先必须的在T型的横梁上
            if( getLVertex(bk, size, ptt[0], ptt[1], pt) &&
                getLVertex(bk, size, ptt[2], ptt[3], pt+2) )
            {
                b=true;
                pt[0] = (pt[0]+pt[2])/2;
                pt[1] = (pt[1]+pt[3])/2;
            }
        }
    }
    else if( type==CTyle )
    {
        if( getCPt(bk, ptt, m, size) == 4 )
        {//十字交点
            Vec4f L1;
            Vec4f L2;
            buildLine(ptt[0], ptt[2], L1);
            buildLine(ptt[1], ptt[3], L2);
            Point2f pp = Cross(L1,L2);
            pt[0] = toint(pp.x);
            pt[1] = toint(pp.y);
            b=true;
        }
    }
    if( b )//成功取得交点
    {
        TLPt tlpt(type,x,y,size);
        tlpt.ox = x+pt[0];
        tlpt.oy = y+pt[1];
        //计算模式,看声明.
        if( type == LTyle )
        {
            if( m[1] == 10 && m[3] == 10 )
                tlpt.m = 1;
            else if( m[1] == 10 && m[2] == 10 )
                tlpt.m = 2;
            else if( m[0] == 10 && m[2] == 10 )
                tlpt.m = 3;
            else
                tlpt.m = 4;
            //计算直线sin,cos
            calcLine(ptt[0],pt,tlpt.line);
            calcLine(ptt[1],pt,tlpt.line+2);
            //计算直线的角度
            tlpt.angle[0] = my_atan(tlpt.line[1],tlpt.line[0]);
            tlpt.angle[1] = my_atan(tlpt.line[3],tlpt.line[2]);
        }
        else if( type ==TTyle )
        {
            if( m[0] == 2 )
                tlpt.m = 1;
            else if( m[1] == 2 )
                tlpt.m = 3;
            else if( m[2] == 2 )
                tlpt.m = 4;
            else
                tlpt.m = 2;
            //计算直线sin,cos
            calcLine(ptt[0],ptt[2],tlpt.line);
            int p13[2];
            p13[0] = (ptt[1][0]+ptt[3][0])/2;
            p13[1] = (ptt[1][1]+ptt[3][1])/2;
            calcLine(pt,p13,tlpt.line+2);
            //计算直线的角度
            tlpt.angle[0] = my_atan(tlpt.line[1],tlpt.line[0]);
            tlpt.angle[1] = my_atan(tlpt.line[3],tlpt.line[2]);
        }
        else if( type == CTyle )
        {
            tlpt.m = 0;
            calcLine(ptt[0],ptt[2],tlpt.line);
            calcLine(ptt[1],ptt[3],tlpt.line+2);
            //计算直线的角度
            tlpt.angle[0] = my_atan(tlpt.line[1],tlpt.line[0]);
            tlpt.angle[1] = my_atan(tlpt.line[3],tlpt.line[2]);
        }
        {
            boost::mutex::scoped_lock lock(mMutex);
            //不能同时写入
            TLs.push_back(tlpt);
        }
    }
    releaseBlock(bk);
}
/*
 p是要检测的数据块的起始地址
 x,y是块的左上角坐标
 block_size是数据块的宽度和高度
 rows是行跨距
 如果成功识别将数据存入TL列表
 */
TLType RgnGrid::RngTL( const uchar* p,int x,int y,int block_size )
{
    int m[4];
    int n2 = 0;//010
    int n0 = 0;//000
    //10=1010 白黑白模式
    //2=10 单一白模式
    m[0] = LineMode(p,0,true,block_size);
    m[1] = LineMode(p,block_size-1,true,block_size);
    m[2] = LineMode(p,0,false,block_size);
    m[3] = LineMode(p,block_size-1,false,block_size);
    /* 先进行快速模式匹配筛选
     T 3个5,1个0
     L 2个5,2个0,并且不对边
     + 4个5
     */
    for( int i = 0;i < 4;i++ )
    {
        if( m[i] == 10 )n2++;
        if( m[i] == 2 )n0++;
    }
    
    if( n2==3 && n0 == 1 )
    {
        if( RngTBlock(p,x,y,m,block_size) )
        {
            return TTyle;
        }
    }
    else if( n2==2 && n0 == 2 )
    {
        //不能是相同方向出现010
        if( (m[0]==2&&m[1]==2)||(m[2]==2&&m[3]==2) )
            return TNothing;
        if( RngLBlock(p,x,y,m,block_size) )
        {
            return LTyle;
        }
    }
    else if( n2==4 )
    {//十字型
        if( RngCBlock(p,x,y,m,block_size) )
            return CTyle;
    }
    return TNothing;
}
bool RgnGrid::RngCBlock(const uchar* p,int x,int y,int m[4],int block_size )
{
    int pt[4][4];
    int ptt[4][2];
    int k = 0;
    uchar* bk = getFreeBlock();
    copyBlockFromMain(p,bk,block_size);
    for( int i = 0;i < 4;++i )
    {
        if( m[i] == 10 ) //010模式
        {
            getEdgePoint(bk,pt[k],i,block_size);
            k++;
        }
    }
    if( distancePt(pt[0])>MAX_LINE_WIDTH||distancePt(pt[1])>MAX_LINE_WIDTH||
       distancePt(pt[2])>MAX_LINE_WIDTH||distancePt(pt[3])>MAX_LINE_WIDTH )
    {
        releaseBlock(bk);
        return false;
    }
    bool ret = false;
    if( Blocks(bk,block_size) == 1 ) //Blocks会破坏bk中的数据
    {//单一块
        copyBlockFromMain(p,bk,block_size);
        int* mode = (int*)getFreeBlock();
        if( getCPt(bk, ptt, m, block_size) == 4 )
        {
            int count = getModes(bk, block_size, ptt[0], ptt[1],mode);
            if(count>0&&mode[0]==13)
            {
                count = getModes(bk, block_size, ptt[2], ptt[3],mode);
                if(count>0&&mode[0]==13)
                {
                    count = getModes(bk, block_size, ptt[0], ptt[3],mode);
                    if(count>0&&mode[0]==13)
                    {
                        count = getModes(bk, block_size, ptt[1], ptt[2],mode);
                        if(count>0&&mode[0]==13)
                        {
                            ret = true;
                        }else ret=false;
                    }else ret=false;
                }else ret=false;
            }
        }
        releaseBlock((uchar*)mode);
    }
    releaseBlock(bk);
    return ret;
}
void RgnGrid::copyBlock(const uchar* p,uchar* ds,int size)
{
    for(int i =0;i<size*size;++i)
        *(ds+i)=*(p+i);
}
//从主检测面上复制一个块,同时返回块中黑色点的数量
int RgnGrid::copyBlockFromMain(const uchar* p,uchar* ds,int block_size)
{
    const uchar* pline;
    uchar* pc = ds;
    int count = 0;
    uchar c;
    for( int i = 0;i < block_size;++i )
    {
        pline = p+i*cols;
        for( int j = 0;j<block_size;++j )
        {
            c = *(pline+j);
            *pc++ = c;
            if( c == 0 )count++;
        }
    }
    return count;
}
void RgnGrid::destoryAllBlock()
{
/*
    boost::mutex::scoped_lock lock(mMutex);
    
    for(vector<uchar*>::iterator i = mRngFreeBlks.begin();i!=mRngFreeBlks.end();++i)
        delete [] *i;
 */
    mRngFreeBlks.clear();
}
//在使用完释放块
void RgnGrid::releaseBlock( uchar* bk)
{
    delete [] bk;
 //   boost::mutex::scoped_lock lock(mMutex);
    
 //   mRngFreeBlks.push_back(bk);
}
//取得一个空闲的块
uchar* RgnGrid::getFreeBlock()
{
    return new uchar[9*search_block_size*search_block_size];
    //对于多线程情况如果使用lock,将大大降低性能
    /*
    uchar *p;
    boost::mutex::scoped_lock lock(mMutex);
    
    if( mRngFreeBlks.empty() )
    {
        //分配大点的空间,在扩展时仍然有效
        p = new uchar[9*search_block_size*search_block_size];
        return p;
    }
    else
    {
        p = mRngFreeBlks.back();
        mRngFreeBlks.pop_back();
    }
    return p;
     */
}
/*
 边上的模式是010,该函数用来确定中间黑色边界点
 p是块数据
 pt是返回的两点x,y,x,y
 edge 0顶边,1底边,2左边,3右边
 */
void RgnGrid::getEdgePoint(uchar* p,int pt[4],int edge,int block_size)
{
    //采用两头挤的方法
    int i;
    uchar* pc;
    if(edge<2)
    {
        if( edge == 0 )
        {
            pc = p;
            pt[1] = 0;
            pt[3] = 0;
        }
        else
        {
            pc = p + (block_size-1)*block_size;
            pt[1] = block_size-1;
            pt[3] = block_size-1;
        }
        for( i = 0;i < block_size;++i )
            if( *(pc+i) == FC )
            {
                pt[0] = i;
                break;
            }
        for( i = block_size-1;i >= 0;--i )
            if( *(pc+i) == FC )
            {
                pt[2] = i;
                break;
            }
    }
    else
    {
        if( edge == 2 )
        {
            pc = p;
            pt[0] = 0;
            pt[2] = 0;
        }
        else
        {
            pc = p + block_size-1;
            pt[0] = block_size-1;
            pt[2] = block_size-1;
        }
        for( i = 0;i < block_size;++i )
            if( *(pc+i*block_size) == FC )
            {
                pt[1] = i;
                break;
            }
        for( i = block_size-1;i >= 0;--i )
            if( *(pc+i*block_size) == FC )
            {
                pt[3] = i;
                break;
            }
    }
}
/*获取直线上的所有点,返回取得点的个数
 */
int RgnGrid::getLine(uchar* p,int p1[2],int p2[2],uchar* dst,int block_size)
{
    int x,y,dx,dy,e,delta;
    bool brev = false;
    uchar* dp = dst;
    dx = p2[0]-p1[0];
    dy = p2[1]-p1[1];
    
    if( abs(dx) > abs(dy) )
    {//横向较长时以x为递增主线
        if( dx>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
            brev = true;
        }
        if( dy>0 )
            delta = 1;
        else
        {
            delta = -1;
            dy = -dy;
        }
        e = dx;
        for( int i = 0;i<=dx;++i )
        {
            *dp++ = *(p+y*block_size+x);
            x++;
            e-=dy;
            if(e<=0)
            {
                y+=delta;
                e+=dx;
            }
        }
    }
    else
    {//纵向较长时以y为递增主线
        if( dy>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
            brev = true;
        }
        if( dx>0 )
            delta = 1;
        else
        {
            delta = -1;
            dx = -dx;
        }
        e = dy;
        for( int i = 0;i<=dy;++i )
        {
            *dp++ = *(p+y*block_size+x);
            y++;
            e-=dx;
            if(e<=0)
            {
                x+=delta;
                e+=dy;
            }
        }
    }
    //按顺序来,如果是逆向就做反转操作
    int count = (int)(dp-dst);
    for( int i = 0;i<count/2;++i )
    {
        uchar c = *(dst+i);
        *(dst+i) = *(dst+count-1-i);
        *(dst+count-1-i) = c;
    }
    return count;
}
/*
 使用Bresenham划线法
 在块p中绘制一条直线,颜色为c
 */
void RgnGrid::drawLine(uchar* p,int p1[2],int p2[2],uchar c,int block_size)
{
    int x,y,dx,dy,e,delta;
    dx = p2[0]-p1[0];
    dy = p2[1]-p1[1];
    if( abs(dx) > abs(dy) )
    {//横向较长时以x为递增主线
        if( dx>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
        }
        if( dy>0 )
            delta = 1;
        else
        {
            delta = -1;
            dy = -dy;
        }
        e = dx;
        for( int i = 0;i<=dx;++i )
        {
            *(p+y*block_size+x) = c;
            x++;
            e-=dy;
            if(e<=0)
            {
                y+=delta;
                e+=dx;
            }
        }
    }
    else
    {//纵向较长时以y为递增主线
        if( dy>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
        }
        if( dx>0 )
            delta = 1;
        else
        {
            delta = -1;
            dx = -dx;
        }
        e = dy;
        for( int i = 0;i<=dy;++i )
        {
            *(p+y*block_size+x) = c;
            y++;
            e-=dx;
            if(e<=0)
            {
                x+=delta;
                e+=dy;
            }
        }
    }
}

//返回中点
void RgnGrid::centerPt(int pt[4],int ptt[2])
{
    ptt[0] = (pt[0]+pt[2])/2;
    ptt[1] = (pt[1]+pt[3])/2;
}
//已知直线的坐标x,计算y
//d = y*cosa-x*sina
float RgnGrid::calcYbyX(float sina,float cosa,float d,float x)
{
    return (d+x*sina)/cosa;
}
//已知直线的坐标y,计算x
float RgnGrid::calcXbyY(float sina,float cosa,float d,float y)
{
    return (y*cosa-d)/sina;
}
int RgnGrid::toint(float v)
{
    int i = (int)v;
    return v-i<0.5?i:i+1;
}
//sina,cosa,d构成直线方程
bool RgnGrid::calcBlockCross(float sina,float cosa,float d,int pt[4],int block_size)
{
    float x,y;
    int c = 0;
    
    if( sina != 0 )
    {//和x axis求交
        x = -d/sina;//calcXbyY(sina, cosa, d, 0);
        if( x >=0&&x<=block_size-1 )
        {
            pt[c] = toint(x);
            pt[c+1] = 0;
            c+=2;
        }
        x = ((block_size-1)*cosa-d)/sina;//calcXbyY(sina, cosa, d, block_size-1);
        if( x >=0&&x<=block_size-1 )
        {
            pt[c] = toint(x);
            pt[c+1] = block_size-1;
            c+=2;
        }
        if( c==4 )return true;
    }
    if( cosa != 0 )
    {//和y axis求交
        y = d/cosa;//calcYbyX(sina, cosa, d, 0);
        if( y>=0&&y<=block_size-1 )
        {
            pt[c] = 0;
            pt[c+1] = toint(y);
            c+=2;
        }
        if( c==4 )return true;
        y = (d+(block_size-1)*sina)/cosa;//calcYbyX(sina, cosa, d, block_size-1);
        if( y>=0&&y<=block_size-1 )
        {
            pt[c] = block_size-1;
            pt[c+1] = toint(y);
            c+=2;
        }
        if( c==4 )return true;
    }

    return false;
}
void RgnGrid::printBlock(const uchar*p,int block_size)
{
    for(int k = 0;k<block_size;++k)
    {
        printf(" %d",k%10);
    }
    putchar('\n');
    for(int i=0;i<block_size;++i)
    {
        printf("%d",i%10);
        for(int j=0;j<block_size;++j)
        {
            if( *(p+i*block_size+j)==FC )
                putchar('*');
            else
                putchar(' ');
            putchar(' ');
        }
        printf("|\n");
    }
}
//返回块中黑色区域,有多少不连通的块
int RgnGrid::Blocks(uchar* p,int block_size)
{
    uchar* pcur,*prev;
    uchar pc = 1;
    uchar* bk = p;
    uchar* pco = getFreeBlock();
    int co = 0;
    prev = 0;
    for(int i=0;i<block_size;++i )
    {
        pcur = bk+i*block_size;
        for(int j=0;j<block_size;++j)
        {
            if( *(pcur+j) == FC )
            {
                bool bf =false;
                //先看看上一个像素
                if( j!= 0 && *(pcur+j-1) != BC )
                {
                    bf = true;
                    *(pcur+j) = *(pcur+j-1);
                }
                else if( j!=0 && i!=0 && *(prev+j-1)!=BC ) //上一个像素的上面像素
                {
                    bf = true;
                    *(pcur+j) = *(prev+j-1);
                }
                else if( i!=0 )
                {
                    int k;
                    for(k = j;k<block_size;++k)
                    {
                        if( *(pcur+k)==FC && *(prev+k)!=BC )
                        {
                            *(pcur+j) = *(prev+k);
                            bf = true;
                            break;
                        }
                        else if( *(pcur+k)!=FC )
                            break;
                    }
                    if(!bf && k < block_size && *(prev+k)!=BC)//如果没发现,向后延伸1
                    {
                        bf = true;
                        *(pcur+j) = *(prev+k);
                    }
                }
                if( !bf ) //没发现任何联通
                { //新分配一个区块号
                    *(pcur+j) = pc++;
                }
                //检查冲突
                if( i!=0 )
                {
                    uchar c0,c1;
                    c0=BC;
                    c1 = *(pcur+j);
                    if( *(prev+j)!=BC && *(prev+j)!=*(pcur+j) )
                    {
                        c0 = *(prev+j);
                    }
                    else if( j!=0&&*(prev+j-1)!=BC && *(prev+j-1)!=*(pcur+j) )
                    {
                        c0 = *(prev+j-1);
                    }
                    else if( j!=block_size-1&&*(prev+j+1)!=BC && *(prev+j+1)!=*(pcur+j))
                    {
                        c0 = *(prev+j+1);
                    }
                    if( c0!=BC )
                    { //加入冲突c0,c1
                        bool isfound = false;
                        if( co>0 )
                        {
                            for(int k=co-1;k>=0;--k)
                            {
                                if( (pco[2*k]==c0&&pco[2*k+1]==c1)||
                                   (pco[2*k]==c1&&pco[2*k+1]==c0) )
                                {
                                    isfound = true;
                                    break;
                                }
                            }
                        }
                        if( !isfound )
                        {
                            pco[2*co] = c0;
                            pco[2*co+1] = c1;
                            co++;
                        }
                    }
                }
            }
        }
        prev = pcur;
    }
    releaseBlock(pco);
    return (pc-1)-co;
}
/*
 取得L的顶点坐标
 如果成功返回true,并且vp给出定点坐标
 p0,p1是L的两条臂的顶点
*/
bool RgnGrid::getLVertex( uchar* p,int block_size,int p0[2],int p1[2],int vp[2] )
{
    int ptt[4];
    float r,sina,cosa,d,tro0,tro1;
    float d0,delta;
	/*
     原点到直线的垂线交点为起点，构造直线方程。
     x = cos(angle) * t - distance * sin(angle)
     y = sin(angle) * t + distance * cos(angle)
     那么t代表从直线上任意点到垂足的距离
     */
    r = sqrt((p1[0]-p0[0])*(p1[0]-p0[0])+(p1[1]-p0[1])*(p1[1]-p0[1]));
    sina = (p1[1]-p0[1])/r;
    cosa = (p1[0]-p0[0])/r;
    d = p0[1]*cosa-p0[0]*sina;
    tro0 = block_size/sqrt(2);
    tro1 = block_size*sqrt(2);
    if((sina>0&&cosa<0)||(sina<0&&cosa>0))//异号
    {//
        tro0 = 0;
        tro1 = (block_size-1)*cosa-(block_size-1)*sina;
    }
    else if((sina>0&&cosa>0)||(sina<0&&cosa<0)) //同号
    {//
        tro0 = (block_size-1)*cosa;
        tro1 = -(block_size-1)*sina;
    }
    else //sina=0 || cosa=0
    { //cosa=0和y平行,sina=0和x平行
        return false;
    }
    if( abs(d-tro0) < abs(d-tro1) )
    {
        d0 = tro1;
    }
    else
    {
        d0 = tro0;
    }
    if( d < d0 )
        delta = 1;
    else
        delta = -1;
    float i = d;
    int* pts = (int*)getFreeBlock();
    uchar* pixs = getFreeBlock();
    int* ptt0 = (int*)getFreeBlock();
    int* ptt1 = (int*)getFreeBlock();
    int ptt_count = 0;
    while(true)
    {
        int count,step;
        uchar cur,prev;
        int prev_x,prev_y;
        if( calcBlockCross(sina,cosa,i,ptt,block_size) )
        {
            step = 0;
            count = getLinePt(ptt,ptt+2,pts);
            for(int i=0;i<count;++i)
            {
                int x = pts[2*i];//x
                int y = pts[2*i+1];//y
                cur = *(p+y*block_size+x);
                if(i!=0)
                {
                    if(step==0&&cur==BC&&prev==FC)//第一个黑到白
                    {
                        step = 1;
                        ptt0[2*ptt_count] = prev_x;
                        ptt0[2*ptt_count+1] = prev_y;
                    }
                    else if(step==1&&cur==FC&&prev==BC)
                    {
                        ptt1[2*ptt_count] = x;
                        ptt1[2*ptt_count+1] = y;
                        step = 2;
                    }
                }
                prev = cur;
                prev_x = x;
                prev_y = y;
            }
            if(step==2)
            { //成功完成搜索
                ptt_count++;
            }
            else
            {
                break;
            }
        }
        i+=delta;
        if( delta > 0 )
        {
            if( i >= d0 )
                break;
        }
        else
        {
            if( i <= d0 )
                break;
        }
    }
    /*
     现在取出了L型,两臂内侧的点集.分别存储在ptt0,ptt1中
     个数为ptt_count
     */
    bool bret = false;

    if( ptt_count> 1 ) //最少要2个点
    {
        Vec4i v0;
        Vec4i v1;
        float t0,t1;
        v0[0] = ptt0[0];
        v0[1] = ptt0[1];
        v0[2] = ptt0[2*(ptt_count-1)];
        v0[3] = ptt0[2*(ptt_count-1)+1];
        v1[0] = ptt1[0];
        v1[1] = ptt1[1];
        v1[2] = ptt1[2*(ptt_count-1)];
        v1[3] = ptt1[2*(ptt_count-1)+1];
        Lines l0(v0),l1(v1);
        if( Cross(l0, l1, t0, t1 ) )
        {
            Point2f pt=l0.point2d(t0);
            vp[0] = toint(pt.x);
            vp[1] = toint(pt.y);
            if( vp[0] >= 0 && vp[0] < block_size &&
               vp[1] >=0 && vp[1] < block_size ) //交点在块里面
            {
                bret = true;
            }
        }
    }
    //元块
    /*
    printBlock(p, block_size);
    FillBlock(p, BC, block_size);
    for( int i = 0;i<ptt_count;i++ )
    {
        int x,y;
        x = ptt0[2*i];
        y = ptt0[2*i+1];
        *(p+y*block_size+x) = FC;
        x = ptt1[2*i];
        y = ptt1[2*i+1];
        *(p+y*block_size+x) = FC;
    }
    printBlock(p, block_size);
    */
    releaseBlock((uchar*)pts);
    releaseBlock(pixs);
    releaseBlock((uchar*)ptt0);
    releaseBlock((uchar*)ptt1);
    return bret;
}
/*
 BlockMode
 */
int RgnGrid::getModes( uchar* p,int block_size,int p0[2],int p1[2],int mode[] )
{
    int ptt[4];
    int mode_count = 0;
    float r,sina,cosa,d,tro0,tro1;
    float d0,delta;
    uchar* pixs;
	/*
     原点到直线的垂线交点为起点，构造直线方程。
     x = cos(angle) * t - distance * sin(angle)
     y = sin(angle) * t + distance * cos(angle)
     那么t代表从直线上任意点到垂足的距离
     */
    r = sqrt((p1[0]-p0[0])*(p1[0]-p0[0])+(p1[1]-p0[1])*(p1[1]-p0[1]));
    sina = (p1[1]-p0[1])/r;
    cosa = (p1[0]-p0[0])/r;
    d = p0[1]*cosa-p0[0]*sina;
    tro0 = block_size/sqrt(2);
    tro1 = block_size*sqrt(2);
    if((sina>0&&cosa<0)||(sina<0&&cosa>0))//异号
    {//
        tro0 = 0;
        tro1 = (block_size-1)*cosa-(block_size-1)*sina;
    }
    else if((sina>0&&cosa>0)||(sina<0&&cosa<0)) //同号
    {//
        tro0 = (block_size-1)*cosa;
        tro1 = -(block_size-1)*sina;
    }
    else //sina=0 || cosa=0
    { //cosa=0和y平行,sina=0和x平行
        return false;
    }
    if( abs(d-tro0) < abs(d-tro1) )
    {
        d0 = tro1;
    }
    else
    {
        d0 = tro0;
    }
    if( d < d0 )
        delta = 1;
    else
        delta = -1;
    pixs = getFreeBlock();
    float i = d;
    while(true)
    {
        int count;
        if( calcBlockCross(sina,cosa,i,ptt,block_size) )
        {
            count = getLine(p,ptt,ptt+2,pixs,block_size);
            mode[mode_count++] = ArrayMode(pixs,count);
        }
        i+=delta;
        if( delta > 0 )
        {
            if( i >= d0 )
                break;
        }
        else
        {
            if( i <= d0 )
                break;
        }
    }
    releaseBlock(pixs);
    return mode_count;
}
int RgnGrid::distancePt( int pt[4] )
{
    return (pt[2] - pt[0]) + (pt[3] - pt[1]);
}
void RgnGrid::FillBlock(uchar* p,uchar c,int block_size)
{
    for(int i =0;i<block_size*block_size;++i )
        *(p+i)=c;
}

/*
 对块进行进一步判断看看块是不是L型拐角,如果是加入到TL表中
 */
bool RgnGrid::RngLBlock(const uchar* p,int x,int y,int m[4],int block_size )
{
    uchar* bk = getFreeBlock();
    copyBlockFromMain(p,bk,block_size);
    int pt[2][4];
    int ptt[4][2];
    int k = 0;
    for( int i = 0;i < 4;++i )
    {
        if( m[i] == 10 ) //010模式
        {
            getEdgePoint(bk,pt[k],i,block_size);
            k++;
        }
    }
    bool ret = false;
    if( distancePt(pt[1])>MAX_LINE_WIDTH||distancePt(pt[0])>MAX_LINE_WIDTH )
    {
        releaseBlock(bk);
        return false;
    }
    if( Blocks(bk,block_size) == 1 )
    {
        copyBlockFromMain(p,bk,block_size);
        //Mat b(block_size,block_size,CV_8UC1,bk);
        int* mode = (int*)getFreeBlock();
        int* smode = (int*)getFreeBlock();
        //printBlock(bk, block_size);
        if( getLPt(bk,ptt,m,block_size) == 2 )
        {
            int count = getModes(bk, block_size, ptt[0], ptt[1],mode);
            int scount = getSimpleMode(mode,count,smode);
            if( scount >= 4 && smode[0]==13 && smode[scount-1]==2 &&
               smode[scount-2]==10&&smode[scount-3]==42)
            {//13,(26,21,42),10,2
                for( k = 1;k<scount-2;++k )
                {
                    if( smode[k]!=26&&smode[k]!=21&&smode[k]!=42 )
                        ret = false;
                }
                ret = true;
            }
            else
                ret =false;
            /*
             在做更加严格的测试,模式顺序测试
             T型的顺序是13,42,10
             */
            if( !oder(smode,scount,13,42,10) )
                ret = false;
        }
        releaseBlock((uchar*)smode);
        releaseBlock((uchar*)mode);
    }
    releaseBlock(bk);
    return ret;
}

int RgnGrid::getSimpleMode(int mode[],int count,int smode[] )
{
    int prev;
    int cur;
    int scount = 0;
    prev = 0;
    for( int i = 0;i<count;++i )
    {
        cur = mode[i];
        if( prev == cur )
            continue;
        smode[scount++] = cur;
        prev = cur;
    }
    return scount;
}
/*
 对块进行进一步判断看看块是不是T型边,如果是加入到TL表中
 */
bool RgnGrid::RngTBlock(const uchar* p,int x,int y,int m[4],int block_size )
{
    int pt[3][4];
    int ptt[4][2];
    int k = 0;
    uchar* bk = getFreeBlock();
    copyBlockFromMain(p,bk,block_size);
    for( int i = 0;i < 4;++i )
    {
        if( m[i] == 10 ) //010模式
        {
            getEdgePoint(bk,pt[k],i,block_size);
            k++;
        }
    }
    if( distancePt(pt[2])>MAX_LINE_WIDTH||distancePt(pt[1])>MAX_LINE_WIDTH||distancePt(pt[0])>MAX_LINE_WIDTH )
    {
        releaseBlock(bk);
        return false;
    }
    bool ret = false;
    if( Blocks(bk,block_size) == 1 ) //Blocks会破坏bk中的数据
    {
        copyBlockFromMain(p,bk,block_size);
       // Mat b(block_size,block_size,CV_8UC1,bk);
        int* mode = (int*)getFreeBlock();
        int* smode = (int*)getFreeBlock();
        if( getLPt(bk,ptt,m,block_size) == 4 )
        {
            for( int i = 0;i<2;++i )
            {
                int count = getModes(bk, block_size, ptt[2*i], ptt[2*i+1],mode);
                int scount = getSimpleMode(mode,count,smode);
                if( scount >= 4 && smode[0]==13 &&
                   (smode[scount-1]==2||smode[scount-1]==6||smode[scount-1]==5||smode[scount-1]==10)&&
                   (smode[scount-2]==6||smode[scount-2]==5||smode[scount-2]==10))
                {//13,(26,21,42,10),(5,6,10),(2,6,5,10)
                    for( k = 1;k<scount-2;++k )
                    {
                        if( smode[k]!=26&&smode[k]!=21&&smode[k]!=42&&smode[k]!=10 )
                            ret = false;
                    }
                    ret = true;
                }
                else
                    ret =false;
                /*
                 在做更加严格的测试,模式顺序测试
                 T型的顺序是13,42,10
                 */
                if( !oder(smode,scount,13,42,10) )
                    ret = false;
                if( !ret )break;
            }
        }
        releaseBlock((uchar*)smode);
        releaseBlock((uchar*)mode);
    }
    releaseBlock(bk);
    return ret;
}
/*
 确保模式的顺序出现a,b,c
 */
bool RgnGrid::oder(int smode[],int scount,int a,int b,int c) //必须是正确的顺序
{
    int step = 0;
    for( int s = 0;s<scount;++s )
    {
        if( smode[s] == a )
        {
            if( step == 3 || step == 2 )return false;
            step=1;
        }
        else if( smode[s] == b )
        {
            if( step == 3 )return false;
            step=2;
        }
        else if( smode[s] == c )
        {
            step=3;
        }
    }
    return true;
}
static void swap_lpt(int ptt[2][2])
{
    int x,y;
    x = ptt[0][0];
    y = ptt[0][1];
    ptt[0][0] = ptt[1][0];
    ptt[0][1] = ptt[1][1];
    ptt[1][0] = x;
    ptt[1][1] = y;
}
/*
 该函数返回十字型的两个L内侧点
 */
int RgnGrid::getCPt(uchar* p,int ptt[4][2],int m[4],int block_size )
{
    int pt[4][4];

    getEdgePoint(p, pt[0],0,block_size);
    getEdgePoint(p, pt[1],3,block_size);
    getEdgePoint(p, pt[2],1,block_size);
    getEdgePoint(p, pt[3],2,block_size);
    ptt[0][0] = pt[0][0];
    ptt[0][1] = pt[0][1];
    ptt[1][0] = pt[1][2];
    ptt[1][1] = pt[1][3];
    ptt[2][0] = pt[2][0];
    ptt[2][1] = pt[2][1];
    ptt[3][0] = pt[3][2];
    ptt[3][1] = pt[3][3];
    return 4;
}
/*
 该函数取得和边缘相交同侧的点
 对于T型保证顺序总是从T上的横梁的点到中间点,在到另一边
 */
int RgnGrid::getLPt(uchar* p,int ptt[4][2],int m[4],int block_size )
{
    int k = 0;
    int pt[2][4];
    if( m[0] == 10 && m[2] == 10 )
    {//左上
        getEdgePoint(p,pt[0],0,block_size);
        getEdgePoint(p,pt[1],2,block_size);
        ptt[k][0] = min(pt[0][0],pt[0][2]);//x
        ptt[k][1] = pt[0][1];//y
        k++;
        ptt[k][0] = pt[1][0]; //x
        ptt[k][1] = min(pt[1][1],pt[1][3]); //y
        k++;
        if(m[3]==10)
            swap_lpt(&ptt[k-2]);
    }
    if( m[0] == 10 && m[3] == 10 )
    {//右上
        getEdgePoint(p,pt[0],0,block_size);
        getEdgePoint(p,pt[1],3,block_size);
        ptt[k][0] = max(pt[0][0],pt[0][2]);//x
        ptt[k][1] = pt[0][1];//y
        k++;
        ptt[k][0] = pt[1][0]; //x
        ptt[k][1] = min(pt[1][1],pt[1][3]); //y
        k++;
        if(m[2]==10)
            swap_lpt(&ptt[k-2]);
    }
    if( m[1] == 10 && m[2] == 10 )
    {//左下
        getEdgePoint(p,pt[0],1,block_size);
        getEdgePoint(p,pt[1],2,block_size);
        ptt[k][0] = min(pt[0][0],pt[0][2]);//x
        ptt[k][1] = pt[0][1];//y
        k++;
        ptt[k][0] = pt[1][0];//x
        ptt[k][1] = max(pt[1][1],pt[1][3]);//y
        k++;
        if(m[3]==10)
            swap_lpt(&ptt[k-2]);
    }
    if( m[1] == 10 && m[3] == 10 )
    {//右下
        getEdgePoint(p,pt[0],1,block_size);
        getEdgePoint(p,pt[1],3,block_size);
        ptt[k][0] = max(pt[0][0],pt[0][2]);//x
        ptt[k][1] = pt[0][1];//y
        k++;
        ptt[k][0] = pt[1][0];//x
        ptt[k][1] = max(pt[1][1],pt[1][3]);//y
        k++;
        if(m[2]==10)
            swap_lpt(&ptt[k-2]);
    }
    return k;
}
/*
 p是要检测的数据块的起始地址
 b等于true横向,b等于false竖向
 n第n行或者列
 返回一个二进制模式
 10表示白(255),11表示全黑(0)
 1010表示开始发现白黑白模式
 */
int RgnGrid::LineMode( const uchar* p,int n,bool b,int block_size )
{
    const uchar* ptr;
    uchar c;
    int ret = 1;
    uchar prev = 1;
    if( b )
    {
        ptr = p + n*cols;
        for( int i = 0;i < block_size;i++ )
        {
            c = *(ptr+i);
            if( c == prev )
                continue;
            ret<<=1;
            if( c == FC )
                ret|=1;
            prev = c;
        }
    }
    else
    {
        ptr = p + n;
        for( int i = 0; i < block_size;i++ )
        {
            c = *(ptr+i*cols);
            if( c == prev )
                continue;
            ret<<=1;
            if( c == FC )
                ret|=1;
            prev = c;
        }
    }
    return ret;
}

/*和上面的函数类似只不过p是一个数组n是数组的长度
 发现该数组的模式
 */
int RgnGrid::ArrayMode(const uchar* p,int n)
{
    int ret = 1;
    uchar c;
    uchar prev = 1;
    for( int i = 0;i < n;i++ )
    {
        c = *(p+i);
        if( c == prev )
            continue;
        ret <<= 1;
        if( c== FC )
            ret |= 1;
        prev = c;
    }
    return ret;
}

int RgnGrid::getLinePt(int p1[2],int p2[2],int dst[])
{
    int x,y,dx,dy,e,delta;
    int* dp = dst;
    dx = p2[0]-p1[0];
    dy = p2[1]-p1[1];
    
    if( abs(dx) > abs(dy) )
    {//横向较长时以x为递增主线
        if( dx>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
        }
        if( dy>0 )
            delta = 1;
        else
        {
            delta = -1;
            dy = -dy;
        }
        e = dx;
        for( int i = 0;i<=dx;++i )
        {
            *dp++ = x;
            *dp++ = y;
            x++;
            e-=dy;
            if(e<=0)
            {
                y+=delta;
                e+=dx;
            }
        }
    }
    else
    {//纵向较长时以y为递增主线
        if( dy>0 )
        {
            x = p1[0];
            y = p1[1];
        }
        else
        {
            x = p2[0];
            y = p2[1];
            dx = -dx;
            dy = -dy;
        }
        if( dx>0 )
            delta = 1;
        else
        {
            delta = -1;
            dx = -dx;
        }
        e = dy;
        for( int i = 0;i<=dy;++i )
        {
            *dp++ = x;
            *dp++ = y;
            y++;
            e-=dx;
            if(e<=0)
            {
                x+=delta;
                e+=dy;
            }
        }
    }
    return (int)(dp - dst)/2;
}
//计算点到直线的垂直距离
float RgnGrid::lineDistance(Vec4f l,int x,int y)
{
    Vec4f l2;
    l2[0] = l[1];
    l2[1] = -l[0];
    l2[2] = x;
    l2[3] = y;
    Point2f cp = Cross(l, l2);
    return distance2(cp,Point2f(x,y));
}
/*
 */
void RgnGrid::LinearSelect()
{
    vector<Vec2i> tlp;
    for( int m = 1;m<=4;++m )
    {
        for( TLVector::iterator i=TLs.begin();i!=TLs.end();++i )
        {
            bool ba = false;
            if((i->type == TTyle && i->m==m))
            {
                ba = true;
            }
         /*   else if( i->type == TLType::LTyle )
            { //将L也加入到对应的点集中一起进行匹配
              //在参考点较少的时候有用
                if( m == 1 && (i->m==1 || i->m==2) )
                    ba = true;
                else if( m == 2 && (i->m==2 || i->m==3) )
                    ba = true;
                else if( m==3&&(i->m==3||i->m==4) )
                    ba = true;
                else if( m==4&&(i->m==1||i->m==4) )
                    ba = true;
            }*/
            if( ba )
            {
                Vec2i v;
                v[0] = i->ox;
                v[1] = i->oy;
                tlp.push_back(v);
            }
        }
        //发现匹配直线
        fitLine(tlp, OuterBorder[m-1], CV_DIST_L1, 0, 0, 0);
        //使用直线来确定那些点在直线上
        for( TLVector::iterator i=TLs.begin();i!=TLs.end();++i )
        {
            if( i->type == TTyle && i->m==m )
            {
                //计算点到直线的距离如果小与search_block_size/2就加入边列表
                if(lineDistance(OuterBorder[m-1],i->ox,i->oy)<search_block_size/2)
                    TBorder[m-1].push_back(*i);
            }
        }
        tlp.clear();
    }
    //匹配四个角
}

static bool angle_tro(float a0,float a1,float tro)
{
    float v = abs(a1-a0);
    return (v<=CV_PI/2)?(v < tro):(CV_PI-v)<tro;
}
//判断pt0于pt1是否共线
//返回0不共线的
//共线的话分别返回1,2,3,4分别代表,直线(0,0)共线,(0,1),(1,0),(1,1)
//每种点有两条对应的直线,(0,0)是第一条和第一条...
int RgnGrid::isLine(const TLPt& pt0,const TLPt& pt1,float tro) const
{
    float ang = my_atan(pt1.oy-pt0.oy,pt1.ox-pt0.ox);
    if( pt0.type==TTyle && pt1.type==TTyle )
    {
        if( angle_tro(ang,pt0.angle[0],tro)&&angle_tro(ang,pt1.angle[0],tro) )
            return 1;
        else if(angle_tro(ang,pt0.angle[1],tro)&&angle_tro(ang,pt1.angle[1],tro))
            return 4;
    }
    else if( pt0.type==TTyle && pt1.type==LTyle )
    {
        if( angle_tro(ang,pt0.angle[0],tro) )
        {
            
            if(angle_tro(ang,pt1.angle[0],tro))
                return 1;
            else if(angle_tro(ang,pt1.angle[1],tro))
                return 2;
        }
    }
    else if( pt1.type==TTyle && pt0.type==LTyle )
    {
        if( angle_tro(ang,pt1.angle[0],tro) )
        {
            
            if(angle_tro(ang,pt0.angle[0],tro))
                return 1;
            else if(angle_tro(ang,pt0.angle[1],tro))
                return 2;
        }
    }
    //L和L不互相应征,和C对C
    else if( (pt1.type==LTyle && pt0.type==LTyle &&
            pt1.m != pt0.m) || (pt0.type==CTyle && pt1.type==CTyle) )
    {
        if( angle_tro(ang, pt0.angle[0], tro) && angle_tro(ang, pt1.angle[0], tro) )
            return 1;
        else if( angle_tro(ang, pt0.angle[0], tro) && angle_tro(ang, pt1.angle[1], tro) )
            return 2;
        else if( angle_tro(ang, pt0.angle[1], tro) && angle_tro(ang, pt1.angle[0], tro) )
            return 3;
        else if( angle_tro(ang, pt0.angle[1], tro) && angle_tro(ang, pt1.angle[1], tro) )
            return 4;
    }
    else if( pt0.type==TTyle && pt1.type==CTyle )
    {
        if( angle_tro(ang,pt0.angle[1],tro) )
        {
            
            if(angle_tro(ang,pt1.angle[0],tro))
                return 1;
            else if(angle_tro(ang,pt1.angle[1],tro))
                return 2;
        }
    }
    else if( pt1.type==TTyle && pt0.type==CTyle )
    {
        if( angle_tro(ang,pt1.angle[1],tro) )
        {
            
            if(angle_tro(ang,pt0.angle[0],tro))
                return 1;
            else if(angle_tro(ang,pt0.angle[1],tro))
                return 2;
        }
    }
    return 0;
}
/*
    vps表示多个共线组,每一个vps项都是一个共线点的列表.
    函数VPS,将pt和vps中的不同分组进行共线比较.如果发现合适就返回true,否则返回false
 */
bool RgnGrid::VPS(vector<TLVector>& vps,TLPt& pt,float tro)
{
    for(vector<TLVector>::iterator i=vps.begin();i!=vps.end();++i )
    {
        int tc = 0;
        int err = 0;

        for( TLVector::iterator j = i->begin();j!=i->end();++j )
        {
            if( isLine(pt,*j,tro) )
            {
                tc++;
            //    i->push_back(pt);
            //    return true;
            }
            else err++;
        }
  //      if( tc>err/2 ) //如果共线的次数大于不共线的一半
        if( tc>err )
        {
            i->push_back(pt);
            return true;
        }
    }
    return false;
}
static bool sort_x( const TLPt& p1,const TLPt& p2 )
{
    return p1.ox < p2.ox;
}
static bool sort_y( const TLPt& p1,const TLPt& p2 )
{
    return p1.oy < p2.oy;
}
static void AverageVector(vector<TLPt>& edge,float avg[2])
{
    int count = 0;
    avg[0] = 0;
    avg[1] = 0;
    for(vector<TLPt>::iterator i=edge.begin();i!=edge.end();++i)
    {
        if( i->type==TTyle )
        {
            avg[0] += i->line[2];
            avg[1] += i->line[3];
            count++;
        }
    }
    avg[0] /= count;
    avg[1] /= count;
}
static float Subdot(float p1[2],float p2[2])
{
    return sqrt((p2[0]-p1[0])*(p2[0]-p1[0])+(p2[1]-p1[1])*(p2[1]-p1[1]));
}
/*
//找到vp的两个端点返回
static Vec4f segment(const TLVector& vp)
{
}
//线段seg1,和线段seg2,比较角度>tro返回false
//<tro的交点在两线段内部的返回true
static bool isSegLine(Vec4f seg1,Vec4f seg2,float tro)
{
    return false;
}
 */
/*对边edge进行挑选
    首先将共线点进行分类,然后挑选最佳的分类
    最后对重复的点进行合并
 
 edge,vps,是返回的数据,edge是最多的T型边.vps是全部分组.
*/
void RgnGrid::SelectEdge(list<TLPt>& edge,vector<TLVector>& vps,float tro,int bs)
{
   // vector<TLVector> vps;
    TLVector vtmp;
    /*
      vps 是一个列表,每一项存储一组共线点(使用isLine判断出来的)
     */
    list<TLPt>::iterator cur = edge.begin();
    while(cur!=edge.end())
    {
        if( !VPS(vps,*cur,tro) )
        {//没有在存在的类中发现
            list<TLPt>::iterator next = cur;
            next++;
            //从当前位置向后查找,直到发现一个共线点(isLine判断的)
            for(list<TLPt>::iterator it=next;it!=edge.end();++it)
            {
                if( isLine(*cur, *it, tro) )
                {
                    TLVector vp;
                    vp.push_back(*cur);
                    vp.push_back(*it);
                    vps.push_back(vp);
                    edge.erase(it);
                    break;
                }
            }
        }
        edge.erase(cur);
        cur = edge.begin();
    }
    
    edge.clear();
    /*
     对那些因为镜头的畸变导致的分类进行合并,它们共同点是T型分组的匹配直线之间角度较小.
     同时交点在两个T型边上.将符合上面特点的T型边合并.
     */
    //暂时没有增加
    /*
    for(int i=0;i<vps.size();++i )
    {
        for(int j=i+1;j<vps.size();++j)
        {
            if( vps.at(i).size()>1 && vps.at(j).size()>1 )
            {
                Vec4f seg1 = segment(vps.at(i));
                Vec4f seg2 = segment(vps.at(j));
                if( isSegLine(seg1,seg2,3*tro) )
                {
                    //合并
                }
            }
        }
    }*/
    //对全部的T型边分类进行合并,该合并指的是在一条T型边中靠的非常近的点.
    for( int i = 0;i < vps.size();++i )
    {
        int m = 0;
        for( TLVector::iterator k=vps.at(i).begin();k!=vps.at(i).end();++k )
        {
            if( k->type==TTyle )
                m = k->m; //的到方位类型
            vtmp.push_back(*k);
        }
        
        //合并重复点
        if( m==1 || m==3 )
        {
            PtSelect(vtmp,edge,bs,true);
        }
        else if( m==2 || m==4 )
        {
            PtSelect(vtmp,edge,bs,false);
        }
        //合并完成的点重新放入到vps中去
        vps.at(i).clear();
        for(list<TLPt>::iterator it = edge.begin();it!=edge.end();++it)
        {
            vps.at(i).push_back(*it);
        }
        vtmp.clear();
        edge.clear();
    }
}
/*
    vtmp中靠近的点进行提取那个最佳的到edge中
    vtmp中包含作一条边的T型点和L型点.它们有些非常靠近,其实是相同的点.
    b代表对横向或者纵向的选择
 */
void RgnGrid::PtSelect(vector<TLPt>& vtmp,list<TLPt>& edge,int bs,bool b)
{
    float avg[2];
    if( b )
        std::sort(vtmp.begin(),vtmp.end(),sort_x);
    else
        std::sort(vtmp.begin(),vtmp.end(),sort_y);
    AverageVector(vtmp,avg);
    for( TLVector::iterator i = vtmp.begin();i!=vtmp.end();++i )
    {
        if( i->type==TTyle )
        {
            bool bdo=false;
            float subdot;
            float min_subdot = FLT_MAX;
            TLVector::iterator max_it=vtmp.end();
            TLVector::iterator last_it;
            for(TLVector::iterator j = i;j!=vtmp.end();++j )
            {
                if( j->type == TTyle )
                {
                    if( b )
                    {
                        if( abs(i->ox-j->ox) >= bs )
                        {
                            if( max_it!=vtmp.end() )
                            {
                                bdo = true;
                                edge.push_back(*max_it);
                            }
                            i = last_it;
                            break;
                        }
                    }
                    else
                    {
                        if( abs(i->oy-j->oy) >= bs )
                        {
                            if( max_it!=vtmp.end() )
                            {
                                bdo = true;
                                edge.push_back(*max_it);
                            }
                            i = last_it;
                            break;
                        }
                    }
                    subdot = Subdot(j->line+2,avg);
                    if( subdot < min_subdot )
                    {
                        min_subdot = subdot;
                        max_it = j;
                    }
                }
                else if(j->type==LTyle)
                    edge.push_back(*j);
                last_it = j;
            }
            if(!bdo && max_it!=vtmp.end() ) //最后的补上
            {
                edge.push_back(*max_it);
                i = last_it;
            }
        }
        else if( i->type==LTyle )
            edge.push_back(*i);
    }
}
bool RgnGrid::isInVP(TLVector& vp,TLPt& pt )
{
    for(TLVector::iterator i=vp.begin();i!=vp.end();++i)
    {
        if( i->ox == pt.ox && i->oy == pt.oy )
            return true;
    }
    return false;
}
//将L型拐角和边上的T型,一一进行测试,看看它们T型的上横线和L到T的连线是否小于阀值tro
void RgnGrid::LRank(TLPt& pt,float tro)
{
    assert(pt.type==LTyle);
    if( pt.m==1||pt.m==2 )
    {
        for(TLVector::iterator i=TBorder[0].begin();i!=TBorder[0].end();++i )
        {
            if( isLine(*i,pt,tro) )
                pt.rank++;
            else
                pt.fail_rank++;
        }
    }
    if( pt.m==2||pt.m==3 )
    {
        for(TLVector::iterator i=TBorder[1].begin();i!=TBorder[1].end();++i )
        {
            if( isLine(*i,pt,tro) )
                pt.rank++;
            else
                pt.fail_rank++;
        }
    }
    if( pt.m==3||pt.m==4 )
    {
        for(TLVector::iterator i=TBorder[2].begin();i!=TBorder[2].end();++i )
        {
            if( isLine(*i,pt,tro) )
                pt.rank++;
            else
                pt.fail_rank++;
        }
    }
    if( pt.m==4||pt.m==1 )
    {
        for(TLVector::iterator i=TBorder[3].begin();i!=TBorder[3].end();++i )
        {
            if( isLine(*i,pt,tro) )
                pt.rank++;
            else
                pt.fail_rank++;
        }
    }
}

static bool size_px(const TLVector& p1,const TLVector& p2)
{
    return p1.size() > p2.size();
}
//返回T型边的两个最外侧点,组成一个线段返回
//假设v是经过排序的
static Vec4i segment_pt(const TLVector& v)
{
    Vec4i p;
    p[0] = v.at(0).ox;
    p[1] = v.at(0).oy;
    p[2] = v.at(v.size()-1).ox;
    p[3] = v.at(v.size()-1).oy;
    return p;
}
//seg是一个线段的两个顶点
//测试p1,p2在seg上(true),或者在seg外(false)
//b=true横向,b=false纵向
static bool inSegment(Vec4i seg,Point2f p1,Point2f p2,bool b)
{
    float mx,mi;
    if(b)
    {
        mx = max(seg[0],seg[2]);
        mi = min(seg[0],seg[2]);
        if(p1.x > mi && p1.x < mx &&
           p2.x > mi && p2.x < mx )
            return true;
    }
    else
    {
        mx = max(seg[1],seg[3]);
        mi = min(seg[1],seg[3]);
        if(p1.y > mi && p1.y < mx &&
           p2.y > mi && p2.y < mx )
            return true;
    }
    return false;
}
/*
    计算四条T型边的组合评价值,越高越好,不能达成返回-2
    评价主要考虑,1等边性,四条边越是相等分值越高.
 */
int RgnGrid::clac_match_value(const TLVector& v1,const TLVector& v2,
                            const TLVector& v3,const TLVector& v4)
{
    if( v1.size()<2||v2.size()<2||v3.size()<2||v4.size()<2 )
        return -2;
    Vec4i p1 = segment_pt(v1);
    Vec4i p2 = segment_pt(v2);
    Vec4i p3 = segment_pt(v3);
    Vec4i p4 = segment_pt(v4);
    Vec4f L1,L2,L3,L4; //4条直线
    buildLine(&p1[0],&p1[2],L1);
    buildLine(&p2[0],&p2[2],L2);
    buildLine(&p3[0],&p3[2],L3);
    buildLine(&p4[0],&p4[2],L4);
    Point2f pt1,pt2,pt3,pt4; //4个交点
    pt1 = Cross(L1,L4);
    pt2 = Cross(L1,L2);
    pt3 = Cross(L2,L3);
    pt4 = Cross(L3,L4);
    float l1,l2,l3,l4; //四条边长
    l1 = distance2(pt1, pt2);
    l2 = distance2(pt2, pt3);
    l3 = distance2(pt3, pt4);
    l4 = distance2(pt4, pt1);
    /*
        使用最短的那根当1,来计算其他的差值.然后除18(一个等边权重)
        当然数量T型点数量也是一个考虑因素
        负面的评价,如果一条直线,穿越其他的线段中间.将导致评价值降低
     p1-l1--p2
     |      |
     l4     l2
     |      |
     p4-l3--p3
     */
    //这里不做复杂的计算,而是简单的看看交点在线段内还是在线段外.
    //在线段内返回true,在线段外返回false
    bool b1 = inSegment(p1,pt1,pt2,true);
    bool b2 = inSegment(p2,pt2,pt3,false);
    bool b3 = inSegment(p3,pt3,pt4,true);
    bool b4 = inSegment(p4,pt1,pt4,false);
    float l_min = min(min(min(l1,l2),l3),l4);
    float va = (l1-l_min)/l_min+(l2-l_min)/l_min+(l3-l_min)/l_min+(l4-l_min)/l_min;
    float vb = (b1?36:0) + (b2?36:0) + (b3?36:0) + (b4?36:0);
    float v = 18/va>156?156:18/va + v1.size()+v2.size()+v3.size()+v4.size() - vb;
    return (int)v;
}
/*
    已知TBorders,找到最佳的4条T型边.
 */
void RgnGrid::MatchEdge(list<TLPt> edge[4])
{
    /*
     //该算法仅仅将最多的边当最佳
    for(int i=0;i<4;++i)
    {
        int max_size = -1;
        int max_index = -1;
        edge[i].clear();
        for(int j=0;j<TBorders[i].size();++j)
        {
            if( max_size < (int)TBorders[i].at(j).size() )
            {
                max_size = (int)TBorders[i].at(j).size();
                max_index = j;
            }
        }
        if( max_index>=0 )
        {
            for(int k=0;k<TBorders[i].at(max_index).size();++k)
                edge[i].push_back(TBorders[i].at(max_index).at(k));
        }
    }
     */
    /*
        将每条边的分组进行组合,然后评价,得出评价值.取得评价最高的组合返回
        评价值计算方法:将组合的T型边匹配直线求出,然后用这些直线分割其他三条边,
        正常情况下分割结果是这些T型点都在一侧.如果它们分布在两侧将被视为不正确.
        以此为依据来计算评价值. (如果评价值相同取T型边数量较多者.)
     */
    //确保TBorders按从多到少的顺序进行排序
    for(int i=0;i<4;++i)
    {
        sort(TBorders[i].begin(),TBorders[i].end(),size_px);
    }
    /*
     //该算法仅仅将最多的边当最佳
    for(int i=0;i<4;++i)
    {
        edge[i].clear();
        if( !TBorders[i].at(0).empty() )
        {
            edge[i].resize(TBorders[i].at(0).size());
            copy(TBorders[i].at(0).begin(),TBorders[i].at(0).end(),edge[i].begin());
        }
    }*/
    //
    int i_max,j_max,k_max,s_max;
    int value = -1;
    for(int i=0;i<TBorders[0].size();++i)
    {
        for(int j=0;j<TBorders[1].size();++j)
        {
            for(int k=0;k<TBorders[2].size();++k)
            {
                for(int s=0;s<TBorders[3].size();++s)
                {
                    //计算评价值
                    int v = clac_match_value(TBorders[0].at(i),TBorders[1].at(j),
                                             TBorders[2].at(k),TBorders[3].at(s));
                    if( v > value )
                    {
                        value = v;
                        i_max = i;
                        j_max = j;
                        k_max = k;
                        s_max = s;
                    }
                }
            }
        }
    }
    if( value != -1 )
    { //将最大评级的边复制到edge中去
        for(int i=0;i<4;++i)
            edge[i].clear();
        edge[0].resize(TBorders[0].at(i_max).size());
        copy(TBorders[0].at(i_max).begin(),TBorders[0].at(i_max).end(),edge[0].begin());
        edge[1].resize(TBorders[1].at(j_max).size());
        copy(TBorders[1].at(j_max).begin(),TBorders[1].at(j_max).end(),edge[1].begin());
        edge[2].resize(TBorders[2].at(k_max).size());
        copy(TBorders[2].at(k_max).begin(),TBorders[2].at(k_max).end(),edge[2].begin());
        edge[3].resize(TBorders[3].at(s_max).size());
        copy(TBorders[3].at(s_max).begin(),TBorders[3].at(s_max).end(),edge[3].begin());
    }
    else
    { //没找到匹配返回最多的边
        for(int i=0;i<4;++i)
        {
            edge[i].clear();
            if( !TBorders[i].at(0).empty() )
            {
                edge[i].resize(TBorders[i].at(0).size());
                copy(TBorders[i].at(0).begin(),TBorders[i].at(0).end(),edge[i].begin());
            }
        }
    }
}
/*
    如果TBorder边上的点在匹配出来的直线OuterBorder组成的矩形外面,删除这些外面的点
    然后返回true.
*/
bool RgnGrid::hasTPtOutAndRemove()
{
    //如果不完整直接返回false
    for(int i=0;i<4;++i)
        if(Intact[i]==1)return false;
    Point2f pt[4];
    pt[0] = Cross(OuterBorder[0],OuterBorder[3]);
    pt[1] = Cross(OuterBorder[0],OuterBorder[1]);
    pt[2] = Cross(OuterBorder[1],OuterBorder[2]);
    pt[3] = Cross(OuterBorder[2],OuterBorder[3]);
    bool b = false;
    //上横线
    for(TLVector::iterator it=TBorder[0].begin();it!=TBorder[0].end();)
    {
        int x = it->ox;
        if( x > pt[1].x || x < pt[0].x )
        {
            b = true;
            it = TBorder[0].erase(it);
        }
        else
            ++it;
    }
    //下横线
    for(TLVector::iterator it=TBorder[2].begin();it!=TBorder[2].end();)
    {
        int x = it->ox;
        if( x > pt[2].x || x < pt[3].x )
        {
            b = true;
            it = TBorder[2].erase(it);
        }
        else
            ++it;
    }
    //左竖线
    for(TLVector::iterator it=TBorder[3].begin();it!=TBorder[3].end();)
    {
        int y = it->oy;
        if( y > pt[3].y || y < pt[0].y )
        {
            b = true;
            it = TBorder[3].erase(it);
        }
        else
            ++it;
    }
    //右竖线
    for(TLVector::iterator it=TBorder[1].begin();it!=TBorder[1].end();)
    {
        int y = it->oy;
        if( y > pt[2].y || y < pt[1].y )
        {
            b = true;
            it = TBorder[1].erase(it);
        }
        else
            ++it;
    }    
    return b;
}
/*
  使用角度阀值进行选择.
    方法如下:考虑同侧边应该在一条直线上,对同侧进行分类.然后挑出最佳的那条
    函数用来确定TBorder,Intact,OuterBorder
    该函数的一个问题是没有利用十字型点信息.仅仅靠T型点进行处理.
 */
void RgnGrid::SelectMatch(float tro)
{
    list<TLPt> edge[4];

    for(int m=1;m<=4;++m)
    {
        for(TLVector::iterator i=TLs.begin();i!=TLs.end();++i)
        {
            if(i->type==TTyle && i->m==m)
            {
                edge[m-1].push_back(*i);
            }
            else if(i->type==LTyle )
            {
                if( m==1 &&(i->m==1||i->m==2) )
                    edge[m-1].push_back(*i);
                else if( m==2 &&(i->m==2||i->m==3) )
                    edge[m-1].push_back(*i);
                else if( m==3 &&(i->m==3||i->m==4) )
                    edge[m-1].push_back(*i);
                else if( m==4 &&(i->m==4||i->m==1) )
                    edge[m-1].push_back(*i);
            }
        }
        SelectEdge(edge[m-1],TBorders[m-1],tro,search_block_size);
    }
    //匹配出最佳的边
    MatchEdge(edge);
    /*
        上面对TBorders进行了填充,TBorders其中包括了每条表的所有可能T型边.
        下面找出最佳的匹配4条T型边,填充到edge中去.
     */
    TLVector ls[4];
    //对L型进行选择
    for(int m=1;m<=4;++m )
    {
        for(list<TLPt>::iterator i=edge[m-1].begin();i!=edge[m-1].end();++i)
        {
            if( i->type == LTyle &&
               i->m == m &&
               !isInVP(ls[m-1],*i) )
            {
                ls[m-1].push_back(*i);
            }
            if( i->type == TTyle )
                TBorder[m-1].push_back(*i);
        }
    }
    //先rank
    for(int m=1;m<=4;++m )
    {
        for(TLVector::iterator i=ls[m-1].begin();i!=ls[m-1].end();++i)
            LRank(*i,tro);
    }
    //再找出最大的
    for(int m=1;m<=4;++m )
    {
        int max_rank = -1;
        size_t index = 0;
        for(TLVector::iterator i=ls[m-1].begin();i!=ls[m-1].end();++i)
        {
            if( i->rank > max_rank )
            {
                max_rank = i->rank;
                index = i-ls[m-1].begin();
            }
        }
        if( max_rank > 0 )
        {
            Corner[m-1] = ls[m-1].at(index);
        }
    }
    /*
     匹配外轮廓线,4条线
     */
    int loopc = 0;
    do{
        for(int m=1;m<=4;++m )
        {
            Vec2i v2i;
            vector<Vec2i> tlp;
            for(TLVector::iterator i=TBorder[m-1].begin();i!=TBorder[m-1].end();++i)
            {
                v2i[0] = i->ox;
                v2i[1] = i->oy;
                tlp.push_back(v2i);
            }
            //加入定点
            if( m==1 )
            {
                addCornerV2i(tlp,1);
                addCornerV2i(tlp,2);
            }
            else if(m==2)
            {
                addCornerV2i(tlp,2);
                addCornerV2i(tlp,3);
            }
            else if(m==3)
            {
                addCornerV2i(tlp,3);
                addCornerV2i(tlp,4);
            }
            else
            {
                addCornerV2i(tlp,4);
                addCornerV2i(tlp,1);
            }
            OuterBorder[m-1][0] = 0;
            OuterBorder[m-1][1] = 0;
            OuterBorder[m-1][2] = 0;
            OuterBorder[m-1][3] = 0;
            if(tlp.size()>=2)
            {
                Intact[m-1] = 0;
                fitLine(tlp, OuterBorder[m-1], CV_DIST_L1, 0, 0, 0);
            }
            else
                Intact[m-1] = 1;
            tlp.clear();
        }
        ++loopc;
    }
    //下面的代码解决这样的问题,如果有些T点在匹配出来的四条边的外面,删除这些点然后在重新匹配.
    while(loopc<2&&hasTPtOutAndRemove());
    /*
        对十字型点相同点进行合并,放入CrossPt中
     */
    for(TLVector::iterator q=TLs.begin();q!=TLs.end();++q)
    {
        if(q->type==CTyle&&!isInCrossPt(*q))
            CrossPt.push_back(*q);
    }
}
bool RgnGrid::isInCrossPt(TLPt& pt)
{
    for(TLVector::iterator i=CrossPt.begin();i!=CrossPt.end();++i)
    {
        if( abs(pt.ox-i->ox)<helf_block_size&&
           abs(pt.oy-i->oy)<helf_block_size)
        {
            return true;
        }
    }
    return false;
}
//p0-p1组成的直线和p2-p3组成的直线的交点在p0-p1-p2-p3的极值范围内返回true,否则false
static bool crossPtInRang(const TLPt& p0,const TLPt& p1,const TLPt& p2,const TLPt& p3)
{
    int pt1[2];
    int pt2[2];
    Vec4f L1,L2;
    pt1[0] = p0.ox;
    pt1[1] = p0.oy;
    pt2[0] = p1.ox;
    pt2[1] = p1.oy;
    buildLine(pt1, pt2, L1);
    pt1[0] = p2.ox;
    pt1[1] = p2.oy;
    pt2[0] = p3.ox;
    pt2[1] = p3.oy;
    buildLine(pt1,pt2,L2);
    Point2f cpt = Cross(L1,L2);
    Point2f minpt,maxpt;
    minpt.x = min(p0.ox,min(p1.ox,min(p2.ox,p3.ox)));
    minpt.y = min(p0.oy,min(p1.oy,min(p2.oy,p3.oy)));
    maxpt.x = max(p0.ox,max(p1.ox,max(p2.ox,p3.ox)));
    maxpt.y = max(p0.oy,max(p1.oy,max(p2.oy,p3.oy)));
    if( cpt.x>minpt.x && cpt.x<maxpt.x &&
       cpt.y>minpt.y && cpt.y<maxpt.y )
        return true;
    return false;
}
//如果ptl和某个分类匹配就放入其中,并且返回true.否则返回false
bool RgnGrid::PtLineVP(vector<vector<PtLine > >&vps,PtLine& ptl,float tro)
{
    for(vector<vector<PtLine > >::iterator i=vps.begin();i!=vps.end();++i)
    {
        //和第一个比较就行了吧?
        if( !i->empty() )
        {
            PtLine& pl = i->front();
            if( angle_tro(pl.angle, ptl.angle, tro) &&
                crossPtInRang(LLPts[ptl.idx[0]],LLPts[ptl.idx[1]],
                              LLPts[pl.idx[0]],LLPts[pl.idx[1]]))
            {
                i->push_back(ptl);
                return true;
            }
        }
    }
    return false;
}
/*
 类似于SelectMatch,但是使用的数据和方法不同.
 SelectMatch2采用发现十字型点和T型点的方法进行.考虑一组共线的T型点和十字型点.
 T型点定位边界,十字型点定位分割线.从而可以推测出包围线和上面的等距点.
 而SelectMatch仅仅使用T型点的信息.
 考虑到SelectMatch已经做的工作.数据来源T型点从TBorders中引入,十字点从CrossPt中引入
 同样输出一组边OuterBorder直线.
 */
void RgnGrid::SelectMatch2(float tro)
{
    //先将点都收集到LLPts中去
    
    for(TLVector::iterator it=CrossPt.begin();it!=CrossPt.end();++it)
        LLPts.push_back(*it);
    for(int i=0;i<4;++i)
    {
        for(vector<TLVector>::iterator it=TBorders[i].begin();it!=TBorders[i].end();++it)
            for(TLVector::iterator j=it->begin();j!=it->end();++j)
                LLPts.push_back(*j);
        //将4个角补上
        if(Corner[i].type!=TNothing)
            LLPts.push_back(Corner[i]);
    }
    //下面将分析出经纬线来,LL[0]是竖线,LL[1]是横线,它们都LLPts中的点的索引值
    list<PtLine> ptv;
    for(TLVector::iterator it=LLPts.begin();it!=LLPts.end();++it)
    {
        for(TLVector::iterator j=it+1;j!=LLPts.end();++j)
        {
            int c = isLine(*it,*j,tro);
            if( c )
            {
                PtLine ptl;
                ptl.c = c;
                ptl.idx[0] = it-LLPts.begin();
                ptl.idx[1] = j-LLPts.begin();
                ptl.angle = my_atan(j->oy-it->oy, j->ox-it->ox);
                ptv.push_back(ptl);
            }
        }
    }
    /*
     现在将共线的点对都放入到ptv中了,下面将个这些点对进行分类
     将向平行或者角度很接近的进行归类
    */
    vector<vector<PtLine > > vps;
    while(!ptv.empty())
    {
        PtLine& ptl = ptv.front();
        if( PtLineVP(vps,ptl,tro) )
        { //成功放入分类
            ptv.erase(ptv.begin());
            continue;
        }
        else
        { //没有找到合适的分类
            bool b = false;
            list<PtLine>::iterator next = ptv.begin();
            next++;
            for(list<PtLine>::iterator i=next;i!=ptv.end();++i)
            {
                if(angle_tro(ptl.angle,i->angle,tro)&&
                   crossPtInRang(LLPts[ptl.idx[0]],LLPts[ptl.idx[1]],
                                 LLPts[i->idx[0]],LLPts[i->idx[1]]))
                {
                    vector<PtLine> vp;
                    vp.push_back(ptl);
                    vp.push_back(*i);
                    vps.push_back(vp);
                    ptv.erase(ptv.begin());
                    ptv.erase(i);
                    b = true;
                    break;
                }
            }
            if( !b ) //都没发现
            {
                ptv.erase(ptv.begin());
            }
        }
    }
    /*
        对vps中的直线进行在分类,主要考虑分成横向和纵向两个类
     */
    for(vector<vector<PtLine>>::iterator i=vps.begin();i!=vps.end();++i)
    {
        
    }
}

void RgnGrid::addCornerV2i(vector<Vec2i>& tlp,int m)
{
    if( Corner[m-1].type==LTyle )
    {
        Vec2i v2i;
        v2i[0] = Corner[m-1].ox;
        v2i[1] = Corner[m-1].oy;
        tlp.push_back(v2i);
    }
}
bool RgnGrid::Guess()
{
    for(int i=0;i<4;++i)
    {
        if(Intact[i]==1)
            return GuessIncomplete(0); //不完整模式的处理
    }
    /*
        下面假设能定位完整的4条边.
        首先验证下四个角,或者进行补全
     */
    if( !setCorner(2,OuterBorder[0],OuterBorder[1]) )
        return GuessIncomplete(1);
    if( !setCorner(3,OuterBorder[1],OuterBorder[2]) )
        return GuessIncomplete(1);
    if( !setCorner(4,OuterBorder[2],OuterBorder[3]) )
        return GuessIncomplete(1);
    if( !setCorner(1,OuterBorder[3],OuterBorder[0]) )
        return GuessIncomplete(1);
    
    //对边进行比较互相不能差别太多
    Point2f cp[4];
    for(int m=0;m<4;++m)
    {
        cp[m].x = Corner[m].ox;
        cp[m].y = Corner[m].oy;
    }
    float R[4];
    float R_min,R_max;
    R[0] = distance2(cp[0], cp[1]);
    R[1] = distance2(cp[1], cp[2]);
    R[2] = distance2(cp[2], cp[3]);
    R[3] = distance2(cp[3], cp[1]);
    R_min = min(min(min(R[0],R[1]),R[2]),R[3]);
    R_max = max(max(max(R[0],R[1]),R[2]),R[3]);
    if( R_min/R_max < 0.5f )
        return GuessIncomplete(2);
    //假设是19x19的标准棋盘
    for(int m=0;m<4;++m)
    {
        Point2f p0;
        Point2f p1;

        if( m==0 )
        {
            p0.x = Corner[0].ox;
            p0.y = Corner[0].oy;
            p1.x = Corner[1].ox;
            p1.y = Corner[1].oy;
        }
        else if( m==1 )
        {
            p0.x = Corner[1].ox;
            p0.y = Corner[1].oy;
            p1.x = Corner[2].ox;
            p1.y = Corner[2].oy;
        }
        else if( m==2 )
        {
            p0.x = Corner[2].ox;
            p0.y = Corner[2].oy;
            p1.x = Corner[3].ox;
            p1.y = Corner[3].oy;
        }
        else if( m==3 )
        {
            p0.x = Corner[3].ox;
            p0.y = Corner[3].oy;
            p1.x = Corner[0].ox;
            p1.y = Corner[0].oy;
        }
        //首先对边上的点进行参数化
        if( !eqEdge18(p0,p1,m) )
        {
            GuessIncomplete(3);
        }
    }
    return true;
}
static Point2f Footdrop(Vec4f L,Point2f p)
{
    Vec4f L2;
    L2[0] = L[1];
    L2[1] = -L[0];
    L2[2] = p.x;
    L2[3] = p.y;
    return Cross(L,L2);
}
static float calc_t(Vec4f L,Point2f p)
{
    float t = 0;
    if( L[0]!= 0 )
        t = (p.x-L[2])/L[0];
    else if( L[1]!=0 )
        t = (p.y-L[3])/L[1];
    return t;
}
static Point2f calc_pt(Vec4f L,float t)
{
    Point2f p;
    p.x = L[0]*t+L[2];
    p.y = L[1]*t+L[3];
    return p;
}
static bool float_greater(float a,float b)
{
    return a<b;
}
//看看是不是标准的19x19边
bool RgnGrid::eqEdge18(Point2f p0,Point2f p1,int m )
{
    float r,tro,t,step,last;
    Vec4f L;
    vector<float> fvs;
    vector<float> vs;
    L[0] = p1.x-p0.x;
    L[1] = p1.y-p0.y;
    L[2] = p0.x;
    L[3] = p0.y;
    r = distance2(p0, p1);
    tro = r/18.0f/2.0f;
    step = 1.0f/18.0f;
    //直线L,起点是p0终点p1,参数0-1覆盖整个线段
    fvs.push_back(0); //头p0
    for(TLVector::iterator i=TBorder[m].begin();i!=TBorder[m].end();++i)
    {
        Point2f p(i->ox,i->oy);
        Point2f p2 = Footdrop(L, p); //垂足
        if( distance2(p, p2) < tro ) //别偏离直线太远
        {
            t = calc_t(L,p2);//已知直线和直线的一点,求参数t
            if( t > 0 && t < 1 )
                fvs.push_back(t);
        }
    }
    fvs.push_back(1); //尾p1
    sort(fvs.begin(),fvs.end(),float_greater);
    for(vector<float>::iterator i=fvs.begin();i!=fvs.end();++i)
    {
        int n;
        if( i==fvs.begin() )
        {
            last = *i;
            vs.push_back(last);
            continue;
        }
        t = (*i-last)/step;
        n = toint(t);
        if( n==0 )
        {
            //跳过此点
            continue;
        }
        else if( n==1 )
        {
            last = *i;
            vs.push_back(last);
        }
        else
        { //插入中间点
            for(int k=1;k<=n;++k)
            {
                float s = k/(float)n;
                vs.push_back((*i-last)*s+last);
            }
            last = *i;
        }
    }
    if( vs.size() == 19 )
    {
        for(vector<float>::iterator i=vs.begin();i!=vs.end();++i)
            Edge[m].push_back(calc_pt(L,*i));
        return true;
    }
    else return false;
}
Point2f RgnGrid::eqPoint(Point2f p0,Point2f p1,int n,int i )
{
    Point2f p;
    float r = (float)i/(float)n;
    p.x = (p1.x-p0.x)*r + p0.x;
    p.y = (p1.y-p0.y)*r + p0.y;
    return p;
}
bool RgnGrid::setCorner(int i,Vec4f L0,Vec4f L1)
{
    Point2f p = Cross(L0, L1);
    if( Corner[i-1].type==TNothing )
    {
        Corner[i-1].type=LTyle;
        Corner[i-1].m=i;
        Corner[i-1].ox = toint(p.x);
        Corner[i-1].oy = toint(p.y);
        Corner[i-1].block_size = search_block_size;
        Corner[i-1].line[0] = L0[0];
        Corner[i-1].line[1] = L0[1];
        Corner[i-1].line[2] = L1[0];
        Corner[i-1].line[3] = L1[1];
        return true;
    }
    else
    { //检测,看看两者的距离有多大
        Point2f p2;
        p2.x = Corner[i-1].ox;
        p2.y = Corner[i-1].oy;
        float dis = distance2(p, p2);
        if( dis<search_block_size )
            return true;
        //如果使用LRank选择出来的L拐点和使用边线求交得到的L拐点不一致时
        //进一步看看L拐点的失败次数
        if( Corner[i-1].fail_rank > Corner[i-1].rank )
        {
            //将不采纳LRank算法的到的拐点
            //通过边线重新计算L拐点
            Corner[i-1].type = TNothing;
            return setCorner(i, L0, L1 );
        }
    }
    return false;
}
//猜测一个不完整模式
bool RgnGrid::GuessIncomplete(int type)
{
    return false;
}