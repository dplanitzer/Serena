//
//  Geometry.h
//  Apollo
//
//  Created by Dietmar Planitzer on 3/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Geometry_h
#define Geometry_h

#include "Foundation.h"


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
    Int x, y;
    Int width, height;
} Rect;


extern const Rect Rect_Empty;

static inline Rect Rect_Make(Int x, Int y, Int width, Int height) {
    Rect r;
    r.x = x; r.y = y; r.width = width; r.height = height;
    return r;
}

static inline Rect Rect_Make2(Point origin, Size size) {
    Rect r;
    r.x = origin.x; r.y = origin.y;
    r.width = size.width; r.height = size.height;
    return r;
}

static inline Bool Rect_IsEmpty(Rect r) {
    return r.width <= 0 || r.height <= 0;
}

static inline Bool Rect_Equals(Rect a, Rect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

static inline Point Rect_GetOrigin(Rect r) {
    return Point_Make(r.x, r.y);
}

static inline Size Rect_GetSize(Rect r) {
    return Size_Make(r.width, r.height);
}

static inline Int Rect_GetMinX(Rect r) {
    return r.x;
}

static inline Int Rect_GetMaxX(Rect r) {
    return r.x + r.width;
}

static inline Int Rect_GetMinY(Rect r) {
    return r.y;
}

static inline Int Rect_GetMaxY(Rect r) {
    return r.y + r.height;
}

extern Rect Rect_Union(Rect a, Rect b);
extern Rect Rect_Intersection(Rect a, Rect b);

extern Bool Rect_ContainsPoint(Rect r, Point p);

extern Point Point_ClampedToRect(Point p, Rect r);

#endif /* Geometry_h */
