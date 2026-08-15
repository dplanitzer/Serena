//
//  gd/sprite.h
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#ifndef _GD_SPRITE_H
#define _GD_SPRITE_H 1

#include <gd/types.h>

extern void gdAcquireSprites(gd_device_t _Nonnull device, int basePriority, size_t count, gd_sprite_t _Nullable * _Nonnull pOutSprites);
extern void gdReleaseSprites(gd_sprite_t _Nullable * _Nonnull sprites, size_t count);

extern void gdReleaseSprite(gd_sprite_t _Nullable sprite);

extern void gdGetSpriteConstraints(gd_device_t _Nonnull device, gd_sprite_constraints_t* _Nonnull constraints);

#endif /* _GD_SPRITE_H */
