//
//  sprite.c
//  libgd
//
//  Created by Dietmar Planitzer on 8/14/26.
//  Copyright © 2026 Dietmar Planitzer. All rights reserved.
//

#include "gd_priv.h"
#include <gd/sprite.h>
#include <errno.h>
#include <stdlib.h>

#define MAX_SPRITE_COUNT    8

void gdAcquireSprites(gd_device_t _Nonnull device, int basePriority, size_t count, gd_sprite_t _Nullable * _Nonnull pOutSprites)
{
    struct gd_device* dp = device;
    size_t undo_count = 0;
    int sprite_ids[MAX_SPRITE_COUNT];

    if (count == 0) {
        return;
    }
    if (count > MAX_SPRITE_COUNT) {    //XX define per-platform constant or so
        errno = EINVAL;
        return;
    }

    
    for (size_t i = 0 ; i < count; i++) {
        pOutSprites[i] = NULL;
    }


    if (fd_cntl(dp->fd, GDC_ACQUIRE_SPRITES, basePriority, count, &sprite_ids[0]) < 0) {
        return;
    }


    for (size_t i = 0; i < count; i++) {
        struct gd_sprite* sp = calloc(1, sizeof(struct gd_sprite));

        if (sp == NULL) {
            undo_count = i;
            break;
        }

        sp->dev_d = dp->fd;
        sp->sprite_d = sprite_ids[i];
        pOutSprites[i] = sp;
    }


    if (undo_count > 0) {
        for (size_t i = 0; i < undo_count; i++) {
            free(pOutSprites[i]);
            pOutSprites[i] = NULL;
        }
    }
}

void gdReleaseSprites(gd_sprite_t _Nullable * _Nonnull sprites, size_t count)
{
    struct gd_sprite** sp = (struct gd_sprite**)sprites;

    for (size_t i = 0; i < count; i++) {
        if (sp[i]) {
            fd_cntl(sp[i]->dev_d, GDC_RELEASE_SPRITES, &(sp[i]->sprite_d), 1);
            free(sp[i]);
        }
    }
}


gd_sprite_t _Nullable gdAcquireSprite(gd_device_t _Nonnull device, int priority)
{
    struct gd_device* dp = device;
    int sprite_id[1];

    if (fd_cntl(dp->fd, GDC_ACQUIRE_SPRITES, priority, 1, &sprite_id[0]) < 0) {
        return NULL;
    }


    struct gd_sprite* sp = calloc(1, sizeof(struct gd_sprite));
    if (sp == NULL) {
        fd_cntl(dp->fd, GDC_RELEASE_SPRITES, &sprite_id[0], 1);
        return NULL;
    }

    sp->dev_d = dp->fd;
    sp->sprite_d = sprite_id[0];

    return sp;
}

void gdReleaseSprite(gd_sprite_t _Nullable sprite)
{
    struct gd_sprite* sp = sprite;

    if (sp) {
        fd_cntl(sp->dev_d, GDC_DESTROY_IMAGE, sp->sprite_d);
        free(sp);
    }
}


void gdGetSpriteConstraints(gd_device_t _Nonnull device, gd_sprite_constraints_t* _Nonnull constraints)
{
    struct gd_device* dp = device;

    fd_cntl(dp->fd, GDC_SPRITE_CONSTRAINTS, constraints);
}
