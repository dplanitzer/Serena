//
//  Geometry.c
//  kernel
//
//  Created by Dietmar Planitzer on 3/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Geometry.h"
#include <limits.h>
#include <kern/kernlib.h>


const Point Point_Zero = {0, 0};
const Vector Vector_Zero = {0, 0};
const Size Size_Zero = {0, 0};
const Rect Rect_Empty = {0, 0, 0, 0};
const Rect Rect_Infinite = {INT_MIN, INT_MIN, INT_MAX, INT_MAX};


Rect Rect_Union(Rect a, Rect b)
{
    Rect r;

    r.left = __min(a.left, b.left);
    r.top = __min(a.top, b.top);
    r.right = __max(a.right, b.right);
    r.bottom = __max(a.bottom, b.bottom);
    return r;
}

Rect Rect_Intersection(Rect a, Rect b)
{
    Rect r;

    r.left = __max(a.left, b.left);
    r.top = __max(a.top, b.top);
    r.right = __min(a.right, b.right);
    r.bottom = __min(a.bottom, b.bottom);
    return r;
}

bool Rect_IntersectsRect(Rect a, Rect b)
{
    const int x0 = __max(a.left, b.left);
    const int y0 = __max(a.top, b.top);
    const int x1 = __min(a.right, b.right);
    const int y1 = __min(a.bottom, b.bottom);
    
    return (x1 - x0) > 0 && (y1 - y0) > 0;
}

Point Point_ClampedToRect(Point p, Rect r)
{
    Point rp;
    
    rp.x = __min(__max(p.x, r.left), r.right);
    rp.y = __min(__max(p.y, r.top), r.bottom);    
    return rp;
}
