/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * gGraphData Header
 *
 * Copyright (c) 2011-2014 Mark Watkins <jedimark@users.sourceforge.net>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of the Linux
 * distribution for more details. */

#ifndef GRAPHDATA_H
#define GRAPHDATA_H

#include "graphlayer.h"
#include "SleepLib/day.h"
#include <list>
using namespace std;
/*enum gDataType { gDT_Point, gDT_Point3D, gDT_Stacked, gDT_Segmented };

class gLayer;

class gGraphData
{
public:
    gGraphData(int mp,gDataType t=gDT_Point);
    virtual ~gGraphData();

    virtual void Reload(Day *day=nullptr) { day=day; }

    virtual void Update(Day *day=nullptr);
    //inline wxRealPoint & operator [](int i) { return vpoint[seg][i]; }
    //inline vector<double> & Vec(int i) { return yaxis[i]; }

    //virtual inline const int & NP(int i) { return vnp[i]; }
    //virtual inline const int & MP(int i) { return vsize[i]; }
    inline const gDataType & Type() { return type; }
    virtual double CalcAverage()=0;
    virtual double CalcMinY()=0;
    virtual double CalcMaxY()=0;


    virtual inline double MaxX() { return max_x; }
    virtual inline double MinX() { return min_x; }
    virtual inline double MaxY() { return max_y; }
    virtual inline double MinY() { return min_y; }
    virtual inline void SetMaxX(double v) { max_x=v; if (max_x>real_max_x) max_x=real_max_x; }
    virtual inline void SetMinX(double v) { min_x=v; if (min_x<real_min_x) min_x=real_min_x; }
    virtual inline void SetMaxY(double v) { max_y=v; if (max_y>real_max_y) max_y=real_max_y; }
    virtual inline void SetMinY(double v) { min_y=v; if (min_y<real_min_y) min_y=real_min_y; }
    virtual inline double RealMaxX() { return real_max_x; }
    virtual inline double RealMinX() { return real_min_x; }
    virtual inline double RealMaxY() { return real_max_y; }
    virtual inline double RealMinY() { return real_min_y; }

    virtual inline void SetRealMaxX(double v) { real_max_x=v; }
    virtual inline void SetRealMinX(double v) { real_min_x=v; }
    virtual inline void SetRealMaxY(double v) { real_max_y=v;  }
    virtual inline void SetRealMinY(double v) { real_min_y=v; }

    virtual inline void ForceMinY(double v) { force_min_y=v; }
    virtual inline void ForceMaxY(double v) { force_max_y=v; }

    inline void ResetX() { min_x=real_min_x; max_x=real_max_x; }
    inline void ResetY() { min_y=real_min_y; max_y=real_max_y; }

    virtual inline int VC() { return vc; }
    void SetVC(int v) { vc=v; }

    vector<int> np;
    vector<int> maxsize;
    bool IsReady() { return m_ready; }
    void SetReady(bool b) { m_ready=b; }
    bool isEmpty();

    void AddLayer(gLayer *g);
protected:
    virtual void AddSegment(int max_points) { max_points=max_points; }

    double real_min_x, real_max_x, real_min_y, real_max_y;
    double min_x, max_x, min_y, max_y;

    double force_min_y,force_max_y;

    int vc;
    gDataType type;
    int max_points;
    bool m_ready;

    list<gLayer *> notify_layers;
};

class QPointD
{
public:
    QPointD() {};
    QPointD(double _x,double _y):X(_x),Y(_y) {};
    //QPointD(const QPointD & ref):X(ref.X),Y(ref.Y) {};

    double x() { return X; };
    double y() { return Y; };
    void setX(double v) { X=v; };
    void setY(double v) { Y=v; };
protected:
    double X,Y;
};

class gPointData:public gGraphData
{
public:
    gPointData(int mp);
    virtual ~gPointData();
    virtual void Reload(Day *day=nullptr){ day=day; };
    virtual void AddSegment(int max_points);
    virtual double CalcAverage();
    virtual double CalcMinY();
    virtual double CalcMaxY();

    vector<QPointD *> point;
};
*/
#endif // GRAPHDATA_H
