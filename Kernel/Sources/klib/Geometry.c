//
//  Geometry.c
//  Apollo
//
//  Created by Dietmar Planitzer on 3/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#include "Geometry.h"


const Point Point_Zero = {0, 0};
const Vector Vector_Zero = {0, 0};
const Size Size_Zero = {0, 0};
const Rect Rect_Empty = { 0, 0, 0, 0 };


Rect Rect_Union(Rect a, Rect b)
{
    const Int x0 = __min(a.x, b.x);
    const Int y0 = __min(a.y, b.y);
    const Int x1 = __max(a.x + a.width, b.x + b.width);
    const Int y1 = __max(a.y + a.height, b.y + b.height);
    
    return Rect_Make(x0, y0, __max(x1 - x0, 0), __max(y1 - y0, 0));
}

Rect Rect_Intersection(Rect a, Rect b)
{
    const Int x0 = __max(a.x, b.x);
    const Int y0 = __max(a.y, b.y);
    const Int x1 = __min(a.x + a.width, b.x + b.width);
    const Int y1 = __min(a.y + a.height, b.y + b.height);
    
    return Rect_Make(x0, y0, __max(x1 - x0, 0), __max(y1 - y0, 0));
}

Bool Rect_ContainsPoint(Rect r, Point p)
{
    return p.x >= r.x && p.x < (r.x + r.width) && p.y >= r.y && p.y < (r.y + r.height);
}

Point Point_ClampedToRect(Point p, Rect r)
{
    Point rp;
    
    rp.x = __min(__max(p.x, r.x), r.x + r.width);
    rp.y = __min(__max(p.y, r.y), r.y + r.height);
    
    return rp;
}
