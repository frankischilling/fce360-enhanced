#ifndef FCE360_SNAPSHOT_WORKER_H
#define FCE360_SNAPSHOT_WORKER_H

bool XboxSnapshotInitialize();
void XboxSnapshotShutdown();
bool XboxSnapshotQueue(const unsigned char* pixels, unsigned int height,
    const unsigned char* palette, const char* zeroName);
int XboxSnapshotSave(const unsigned char* pixels, unsigned int height,
    const unsigned char* palette, const char* zeroName);

#endif
