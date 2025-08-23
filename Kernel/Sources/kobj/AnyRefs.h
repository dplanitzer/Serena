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
class_ref(Catalog);
class_ref(ConsoleChannel);
class_ref(Console);
class_ref(DiskBlock);
class_ref(DiskCache);
class_ref(DiskDriver);
class_ref(DispatchQueue);
class_ref(Driver);
class_ref(DriverChannel);
class_ref(DriverManager);
class_ref(FileHierarchy);
class_ref(FileManager);
class_ref(Filesystem);
class_ref(FilesystemManager);
class_ref(FloppyController);
class_ref(FloppyDriver);
class_ref(FSChannel);
class_ref(FSContainer);
class_ref(GamePortController);
class_ref(GraphicsDriver);
class_ref(HIDChannel);
class_ref(HIDManager);
class_ref(Inode);
class_ref(InodeChannel);
class_ref(InputDriver);
class_ref(IOChannel);
class_ref(JoystickDriver);
class_ref(KernFS);
class_ref(KfsDirectory);
class_ref(KfsDevice);
class_ref(KfsProcess);
class_ref(KfsNode);
class_ref(KfsSpecial);
class_ref(KeyboardDriver);
class_ref(LightPenDriver);
class_ref(MouseDriver);
class_ref(PaddleDriver);
class_ref(PipeChannel);
class_ref(Pipe);
class_ref(PlatformController);
class_ref(Process);
class_ref(ProcessManager);
class_ref(ProcChannel);
class_ref(RamDisk);
class_ref(RomDisk);
class_ref(SecurityManager);
class_ref(SerenaFS);
class_ref(SfsDirectory);
class_ref(SfsFile);
class_ref(SfsRegularFile);
class_ref(UResource);
class_ref(VirtualDiskManager);
class_ref(ZorroController);
class_ref(ZorroDriver);

#endif /* AnyRefs_h */
