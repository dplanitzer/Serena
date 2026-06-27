//
//  AnyRefs.h
//  kernel
//
//  Created by Dietmar Planitzer on 9/14/24.
//  Copyright © 2024 Dietmar Planitzer. All rights reserved.
//

#ifndef AnyRefs_h
#define AnyRefs_h

#include <kobj/Class.h>

// This header works around the limitation of typedef in C99 and older where it
// isn't legal to redefine the EXACT SAME type. Just don't get me started on
// this topic...

class_ref(Any);
class_ref(Object);

class_ref(AddressSpace);
class_ref(Console);
class_ref(DiskBlock);
class_ref(DiskCache);
class_ref(DiskContainer);
class_ref(DiskDriver);
class_ref(DisplayDriver);
class_ref(Driver);
class_ref(DriverHandler);
class_ref(FileHierarchy);
class_ref(FileManager);
class_ref(Filesystem);
class_ref(FilesystemManager);
class_ref(FSContainer);
class_ref(Inode);
class_ref(InodeHandler);
#if __IOGPBUS__ > 0
class_ref(IOGPBus);
#endif
class_ref(IOHIDDevice);
class_ref(IOHIDManager);
class_ref(IORegistry);
class_ref(Handler);
class_ref(KernFS);
class_ref(KfsDirectory);
class_ref(KfsNode);
class_ref(KfsSpecial);
class_ref(LogDriver);
class_ref(NullDriver);
class_ref(PipeHandler);
class_ref(Pipe);
class_ref(PlatformController);
class_ref(Process);
class_ref(ProcessManager);
class_ref(RamDisk);
class_ref(RomDisk);
class_ref(SerenaFS);
class_ref(SfsDirectory);
class_ref(SfsFile);
class_ref(SfsRegularFile);
class_ref(VirtualDiskManager);

// Amiga
class_ref(FloppyController);
class_ref(FloppyDriver);
class_ref(GraphicsDriver);
class_ref(AmiJoystick);
class_ref(AmiKeyboard);
class_ref(AmiLightPen);
class_ref(AmiMouse);
class_ref(AmiPaddle);
class_ref(ZorroController);
class_ref(ZorroDriver);
class_ref(ZRamDriver);

#endif /* AnyRefs_h */
