//
//  Geometry.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Geometry_h
#define Geometry_h

#include <klib/klib.h>


typedef struct _Point {
    Int x, y;
} Point;


extern const Point Point_Zero;

static inline Point Point_Make(Int x, Int y) {
    Point pt;
    pt.x = x; pt.y = y;
    return pt;
}

static inline Bool Point_Equals(Point a, Point b) {
    return a.x == b.x && a.y == b.y;
}


////////////////////////////////////////////////////////////////////////////////


typedef struct _Vector {
    Int dx, dy;
} Vector;


extern const Vector Vector_Zero;

static inline Vector Vector_Make(Int dx, Int dy) {
    Vector vec;
    vec.dx = dx; vec.dy = dy;
    return vec;
}

static inline Bool Vector_Equals(Vector a, Vector b) {
    return a.dx == b.dx && a.dy == b.dy;
}


////////////////////////////////////////////////////////////////////////////////


typedef struct _Size {
    Int width, height;
} Size;


extern const Size Size_Zero;

static inline Size Size_Make(Int width, Int height) {
    Size sz;
    sz.width = width; sz.height = height;
    return sz;
}

static inline Bool Size_Equals(Size a, Size b) {
    return a.width == b.width && a.height == b.height;
}


////////////////////////////////////////////////////////////////////////////////


typedef struct _Rect {
    Int left, top;
    Int right, bottom;
} Rect;


extern const Rect Rect_Empty;
extern const Rect Rect_Infinite;

static inline Rect Rect_Make(Int left, Int top, Int right, Int bottom) {
    Rect r;
    r.left = left; r.top = top; r.right = right; r.bottom = bottom;
    return r;
}

static inline Bool Rect_IsEmpty(Rect r) {
    return (r.right <= r.left) || (r.bottom <= r.top);
}

static inline Bool Rect_IsInfinite(Rect r) {
    return (r.right - r.left == INT_MAX) && (r.bottom - r.top == INT_MAX);
}

static inline Bool Rect_Equals(Rect a, Rect b) {
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

static inline Point Rect_GetOrigin(Rect r) {
    return Point_Make(r.left, r.top);
}

// Note that the returned size is limited to (INT_MAX, INT_MAX)
static inline Size Rect_GetSize(Rect r) {
    return Size_Make(r.right - r.left, r.bottom - r.top);
}

// Note that the returned width is limited to INT_MAX
static inline Int Rect_GetWidth(Rect r) {
    return r.right - r.left;
}

// Note that the returned height is limited to INT_MAX
static inline Int Rect_GetHeight(Rect r) {
    return r.bottom - r.top;
}

extern Rect Rect_Union(Rect a, Rect b);
extern Rect Rect_Intersection(Rect a, Rect b);
extern Bool Rect_IntersectsRect(Rect a, Rect b);

static inline Bool Rect_Contains(Rect r, Int x, Int y)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

static inline Bool Rect_ContainsPoint(Rect r, Point p) {
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

extern Point Point_ClampedToRect(Point p, Rect r);

#endif /* Geometry_h */
