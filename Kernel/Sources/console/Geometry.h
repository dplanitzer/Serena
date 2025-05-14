//
//  Geometry.h
//  kernel
//
//  Created by Dietmar Planitzer on 3/5/21.
//  Copyright © 2021 Dietmar Planitzer. All rights reserved.
//

#ifndef Geometry_h
#define Geometry_h

#include <kern/types.h>


typedef struct Point {
    int x, y;
} Point;


extern const Point Point_Zero;

#define Point_Make(__x, __y) \
    ((Point) {__x, __y})

#define Point_Equals(__a, __b) \
    ((__a.x == __b.x && __a.y == __b.y) ? true : false)


////////////////////////////////////////////////////////////////////////////////


typedef struct Vector {
    int dx, dy;
} Vector;


extern const Vector Vector_Zero;

#define Vector_Make(__dx, __dy) \
    ((Vector) {__dx, __dy})

#define Vector_Equals(__a, __b) \
    ((__a.dx == __b.dx && __a.dy == __b.dy) ? true : false)


////////////////////////////////////////////////////////////////////////////////


typedef struct Size {
    int width, height;
} Size;


extern const Size Size_Zero;

#define Size_Make(__width, __height) \
    ((Size) {__width, __height})

#define Size_Equals(__a, __b) \
    ((__a.width == __b.width && __a.height == __b.height) ? true : false)


////////////////////////////////////////////////////////////////////////////////


typedef struct Rect {
    int left, top;
    int right, bottom;
} Rect;


extern const Rect Rect_Empty;
extern const Rect Rect_Infinite;

#define Rect_Decl(__rect, __left, __top, __right, __bottom) \
    Rect __rect = (Rect){__left, __top, __right, __bottom};

#define Rect_Make(__left, __top, __right, __bottom) \
    ((Rect) {__left, __top, __right, __bottom})

#define Rect_IsEmpty(__r) \
    (((__r.right <= __r.left) || (__r.bottom <= __r.top)) ? true : false)

#define Rect_IsInfinite(__r) \
    (((__r.right - __r.left == INT_MAX) && (__r.bottom - __r.top == INT_MAX)) ? true : false)

#define Rect_Equals(__a, __b) \
    ((__a.left == __b.left && __a.top == __b.top && __a.right == __b.right && __a.bottom == __b.bottom) ? true : false)


#define Rect_GetOrigin(__r) \
    Point_Make(__r.left, __r.top)

// Note that the returned size is limited to (INT_MAX, INT_MAX)
#define Rect_GetSize(__r) \
    Size_Make(__r.right - __r.left, __r.bottom - __r.top)

// Note that the returned width is limited to INT_MAX
#define Rect_GetWidth(__r) \
    (__r.right - __r.left)

// Note that the returned height is limited to INT_MAX
#define Rect_GetHeight(__r) \
    (__r.bottom - __r.top)

extern Rect Rect_Union(Rect a, Rect b);
extern Rect Rect_Intersection(Rect a, Rect b);
extern bool Rect_IntersectsRect(Rect a, Rect b);

#define Rect_Contains(__r, __x, __y) \
    ((__x >= __r.left && __x < __r.right && __y >= __r.top && __y < __r.bottom) ? true : false)

#define Rect_ContainsPoint(__r, __p) \
    ((__p.x >= __r.left && __p.x < __r.right && __p.y >= __r.top && __p.y < __r.bottom) ? true : false)

extern Point Point_ClampedToRect(Point p, Rect r);

#endif /* Geometry_h */
