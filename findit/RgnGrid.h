//
//  RgnGrid.h
//  findit
//
//  Created by john on 13-3-9.
//  Copyright (c) 2013年 zuzu. All rights reserved.
//

#ifndef findit_RgnGrid_h
#define findit_RgnGrid_h
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <math.h>
#include <list>
#include "boost/thread.hpp"

using namespace cv;
using namespace std;
//前景背景索引
#define BC 0
#define FC 255
#define MAX_LINE_WIDTH 4

enum TLType
{
    TTyle, //T型
    LTyle, //L型
    CTyle, //交叉点
    TNothing //没有发现
};

struct Lines
{
	Vec4i points;
	float angle; //和x轴的角度，弧度
	float distance; //直线和坐标原点O的距离
	/*
     原点到直线的垂线交点为起点，构造直线方程。
     x = cos(angle) * t - distance * sin(angle)
     y = sin(angle) * t + distance * cos(angle)
     那么t代表从直线上任意点到垂足的距离
     */
	float t0,t1;
	float sina,cosa;
    int flag; //用于具体的算法
    
	Lines(){}
    
    Point2f point2d( const float t )
    {
        Point2f p;
        p.x = cosa*t-distance*sina;
        p.y = sina*t+distance*cosa;
        return p;
    }
    
    //让t0小于t1
    void sort_t()
    {
		if( t1 < t0 )
		{
			float t = t0;
			t0 = t1;
			t1 = t;
		}
    }
    
	float calc_t( float x,float y )
	{
		float t ;
		if( abs(sina) < 0.1 )
			t = (x + distance * sina)/cosa;
		else
			t = (y - distance * cosa)/sina;
		return t;
	}
    
	Lines(const Vec4i& v4):points(v4)
	{
		angle = atan2((float)(points[3]-points[1]),(float)(points[2]-points[0]));
		sina = sin(angle);
		cosa = cos(angle);
		distance = points[1]*cosa-points[0]*sina;
        
		t0 = calc_t( points[0],points[1] );
		t1 = calc_t( points[2],points[3] );
        
        sort_t();
	}
    
	Lines( const float a,const float d,const float _t0,const float _t1 ):
    angle(a),distance(d),t0(_t0),t1(_t1)
	{
        sort_t();
		sina = sin(angle);
		cosa = cos(angle);
		points[0] = cosa*t0-distance*sina;
		points[1] = sina*t0+distance*cosa;
		points[2] = cosa*t1-distance*sina;
		points[3] = sina*t1+distance*cosa;
	}
    
	Lines(const Lines& l ):
    points(l.points),angle(l.angle),
    distance(l.distance),t0(l.t0),t1(l.t1),
    sina(l.sina),cosa(l.cosa)
	{
	}
    
    Lines(const Vec4f& v )
    {
        sina = v[0];
        cosa = v[1];
        distance = v[3]*cosa-v[2]*sina;
    }
};

struct TLPt
{
    int x;
    int y; //块的左上角坐标
    int block_size; //块尺寸
    
    int ox;
    int oy; //LT的交点坐标
    /*两条直线的cos,sin,直线和点ox,oy构成直线方程
     T型第一条直线是T上的横梁,第二条直线是下面的竖线
     */
    float line[4];
    float angle[2]; //两条直线的角度
    int m; //L有1-4种模式,T也有1-4种模式
    /*
     L 1左上角,2右上角,3右下角,4左下角
     T 1向下,2向左,3向上,4向右
     */
    
    TLType type; //块类型
    int rank;
    int fail_rank;
    int idx; //自己在列表中的位置
    int rang[4];//周围和它相邻的共线TLPt点,用在SelectMatch2算法中
    
    TLPt():x(0),y(0),block_size(0),
    ox(0),oy(0),m(0),type(TNothing),rank(0),fail_rank(0),idx(-1)
    {
        rang[0] = -1;
        rang[1] = -1;
        rang[2] = -1;
        rang[3] = -1;
    }
    TLPt(TLType t,int _x,int _y,int bs):
    type(t),x(_x),y(_y),block_size(bs),rank(0),fail_rank(0),idx(-1)
    {
        rang[0] = -1;
        rang[1] = -1;
        rang[2] = -1;
        rang[3] = -1;
    }
};

//一个识别十字,对应一组由十字型点和T型点组成的十字
struct CrossR
{
    Vec4f line[2]; //十字的两条直线,0线是竖线,1线是横线
    TLPt pt; //交点对应的TLPt点
    TLPt rang[4]; //分别对应0=上横线的T型点,1=右竖线T型点
    //2=下横线T型点,如果没有type=TNothing
};

typedef vector<TLPt> TLVector;
typedef boost::function<void (float)> ProgressFunc;

class RgnGrid
{
public:
    RgnGrid();
    
    virtual ~RgnGrid();
    
    void resetThread(bool b);
    void rgnM( Mat& m,bool iswait,ProgressFunc fun );
    Mat getBinaryMat();
    Mat getSourceMat();
    
    void drawTL(Mat& mt,int type);
    void clear();
    
    bool IsMutiCores(){ return mIsMutiCores; }
protected:
    Mat mSrc; //原图
    Mat mDes; //中间图
    Mat mB2;//二值图
    int cols;
    int rows;
    uchar* data;
    /*
        使用多线程加入执行,对于只有一个核心的cpu使用后台执行
     */
    boost::mutex mMu;
    boost::mutex mMu2;
    boost::mutex mMutex;
    boost::condition_variable_any mCondition;
    boost::condition_variable_any mCondition2;
    bool mExit;
    bool mIsMutiCores;
    bool mMainLoopGo;
    bool mMainLoopComplate;
    boost::barrier* mBarrier;
    vector<boost::thread*> mThread;
    vector<Mat> mSrcs; //线程分别处理的区块
    vector<Mat> mDess;
    vector<Mat> mB2s;
    vector<Point2i> mRgnOffsetPts;
    int mOffsetPtsCount;
    int mThread_BlockSize;
    int mThread_h;
    int mThread_w;
    float mProgress;
    float mDeltaProgress;
    ProgressFunc mProgressFunc;
    
    void releaseThread();
    void mainRgn();
    void threadRgn( int i );
    void praperRgnMultiThreadProcess();
    bool getOffsetPt( Point2i& p );
    void threadRgnExpand( int offx,int offy );
    
    int search_block_size;
    int helf_block_size;
    
    TLVector TLs;
    
    bool isInTLs(int x,int y,int size);
    
    /*
     识别棋盘中的T和L型特征
     */
    void Rgn();
    void RgnEdge();
    void GuessGrid();
    
    /*
     p是要检测的数据块的起始地址
     x,y是块的左上角坐标
     block_size是数据块的宽度和高度
     rows是行跨距
     如果成功识别将数据存入TL列表
     */
    TLType RngTL( const uchar* p,int x,int y,int block_size );
    void AddTLPt(TLType type,int x,int y,int size );
    void copyBlock(const uchar* p,uchar* ds,int size);
    //复制一个块,同时返回块中黑色点的数量
    int copyBlockFromMain(const uchar* p,uchar* ds,int block_size);
    /*
     考虑到速度,在开始识别前分配一些内存块
     为多线程优化留下接口
     */
    vector<uchar*> mRngFreeBlks; //空虚表
    
    void destoryAllBlock();
    //在使用完释放块
    void releaseBlock( uchar* bk);
    //取得一个空闲的块
    uchar* getFreeBlock();
    /*
     边上的模式是010,该函数用来确定中间黑色边界点
     p是块数据
     pt是返回的两点x,y,x,y
     edge 0顶边,1底边,2左边,3右边
     */
    void getEdgePoint(uchar* p,int pt[4],int edge,int block_size);
    /*获取直线上的所有点,返回取得点的个数
     */
    int getLine(uchar* p,int p1[2],int p2[2],uchar* dst,int block_size);
    int getLinePt(int p1[2],int p2[2],int dst[]);
    void RgnExpand(int x,int y,int block_size);
    /*
     使用Bresenham划线法
     在块p中绘制一条直线,颜色为c
     */
    void drawLine(uchar* p,int p1[2],int p2[2],uchar c,int block_size);
    bool oder(int mode[],int count,int a,int b,int c); //必须是正确的顺序
    int toint(float v);
    int distancePt( int pt[4] );
    void centerPt(int pt[4],int ptt[2]);
    float calcYbyX(float sina,float cosa,float d,float x);
    float calcXbyY(float sina,float cosa,float d,float y);
    void printBlock(const uchar*p,int block_size);
    void FillBlock(uchar* p,uchar c,int block_size);
    int Blocks(uchar* p,int block_size); //返回块中黑色区域,有多少不联通的块
    int getModes(uchar* p,int block_size,int p1[2],int p2[2],int mode[] );
    bool getLVertex( uchar* p,int block_size,int p0[2],int p1[2],int vp[2] );
    int getSimpleMode(int mode[],int count,int smode[] );
    int getLPt(uchar* p,int pt[4][2],int m[4],int block_size );
    int getCPt(uchar* p,int pt[4][2],int m[4],int block_size );
    /*计算一条直线和块的边缘的交点
     成功返回true,没有交点返回false
    */
    bool calcBlockCross(float sina,float cosa,float d,int pt[4],int block_size);
    /*
     对块进行进一步判断看看块是不是L型拐角,如果是加入到TL表中
     */
    bool RngLBlock(const uchar* p,int x,int y,int m[4],int block_size );
    /*
     对块进行进一步判断看看块是不是T型边,如果是加入到TL表中
     */
    bool RngTBlock(const uchar* p,int x,int y,int m[4],int block_size );
    bool RngCBlock(const uchar* p,int x,int y,int m[4],int block_size );
    /*
     p是要检测的数据块的起始地址
     b等于true横向,b等于false竖向
     n第n行或者列
     返回一个二进制模式
     10表示白(255),11表示全黑(0)
     1010表示开始发现白黑白模式
     */
    int LineMode( const uchar* p,int n,bool b,int block_size);
    /*和上面的函数类似只不过p是一个数组n是数组的长度
     发现该数组的模式
     */
    int ArrayMode(const uchar* p,int n);
    /*
     初步筛选,先对每条边上的点进行排除.那些明显不是一条直线的点
     */
    float lineDistance(Vec4f l,int x,int y); //计算点到直线的距离
    void LinearSelect();
    bool isInCrossPt(TLPt& pt);
    
    /*
        下列函数对TLs中的点进行进一步精选,将它们放入
        四个角Corner[4] 四条边TBorder[4]中去.
        方法如下:先对每侧的边进行共线测试,找出最佳的共线模式
                然后在确定四个拐点,去掉临近的重复点
     */
    void PtSelect(vector<TLPt>& vtmp,list<TLPt>& edge,int bs,bool b);
    void drawTLPt(Mat& mt,const TLPt& pt );
    void LRank(TLPt& pt,float tro);
    bool isInVP(TLVector& vp,TLPt& pt );
    bool VPS(vector<TLVector>& vps,TLPt& pt,float tro);
    int isLine(const TLPt& pt0,const TLPt& pt1,float tro) const;
    int clac_match_value(const TLVector& v1,const TLVector& v2,
                                  const TLVector& v3,const TLVector& v4);
    bool hasTPtOutAndRemove();
    void SelectMatch(float tro);
    void addCrossR(TLPt& pt,vector<TLPt>& vp0,vector<TLPt>& vp1);
    void CrossRang(vector<TLPt>& pts,TLPt& pt,vector<int>& vp0,vector<int> &vp1);
    void SelectMatch2(float tro);
    void SelectEdge(list<TLPt>& edge,vector<TLVector>& vps,float tro,int bs);
    void addCornerV2i(vector<Vec2i>& tlp,int m);
    void MatchEdge(list<TLPt> edge[4]);
    Vec4f OuterBorder[4]; //外边框直线
    TLPt Corner[4]; //是个角点
    vector<TLVector> TBorders[4]; //全部T型边分组.
    TLVector TBorder[4]; //T型边,匹配好的T形边变
    TLVector CrossPt; //C型点
    vector<TLPt> LLPts;
    int Intact[4];
    bool Guess();
    bool GuessIncomplete(int type);
    bool setCorner(int i,Vec4f L0,Vec4f L1);
    Point2f eqPoint(Point2f p0,Point2f p1,int n,int i );
    bool eqEdge18(Point2f p0,Point2f p1,int m );
    vector<Point2f> Edge[4]; //四条表的猜测
};


#endif
