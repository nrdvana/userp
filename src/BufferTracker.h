#ifndef USERP_BUFFERTRACKER_H
#define USERP_BUFFERTRACKER_H

#include <stdint.h>
#include "HeaderDefs.h"
#include "Misc.h"

#ifdef USERP_CXX_IFACE
#include <exception>
namespace Userp {
#endif

/*
 * BufferTracker is a class that maintains a set of buffers, and can efficiently determine
 * which buffer contains a given pointer.  It also maintains a reference count on the buffers.
 * This is a bit less efficient than storing the reference count within a buffer-object, but
 * is more flexible as the buffer can be allocated anywhere by anything, and still be tracked
 * by this class.
 *
 * This class must receive a pointer to the beginning of a buffer when they are registered, but
 * any pointer within the buffer may be used for acquiring and releasing references.  The buffer
 * is registered with a "FreeProc", which is called to release/free/delete the buffer when its
 * reference count reaches 0.  Note that upon registering a buffer its reference count will be 0,
 * but it will not get freed until after it has been acquired for the first time and subsequently
 * reaches a refcount of 0.
 *
 * All buffers must be released and freed before the BufferTracker gets freed.  If not,
 * BufferTracker calls abort().  The logic behind this is that you need to have logic in
 * place to clean things up; it would be bad if the buffers all got freed and then things
 * that were using them started accessing unclaimed memory.  If your program has failed in
 * such a way that you no longer have references to all the buffers, and you still want to
 * clean up the memory that was allocated (for some reason...? keep in mind there's no
 * point in freeing memory if you plan to exit) then you can call FreeAllBuffers(), which
 * will just iterate through the set and call each buffer's free proc regardless of refcnt.
 */
struct userp_buftracker;
typedef struct userp_buftracker userp_buftracker_t;
typedef void (*userp_buftracker_release_proc_t)(const uint8_t *Buffer);

USERP_EXTERN userp_buftracker_t* userp_buftracker_Create();
USERP_EXTERN void userp_buftracker_Destroy(userp_buftracker_t *bt);
USERP_EXTERN void userp_buftracker_FreeAllBuffers(userp_buftracker_t *bt);
USERP_EXTERN const uint8_t *userp_buftracker_BufferOf(userp_buftracker_t *bt, const uint8_t *Pointer);
USERP_EXTERN void userp_buftracker_Register(userp_buftracker_t *bt, const uint8_t *Buffer, int Len, userp_buftracker_release_proc_t FreeProc);
USERP_EXTERN uint8_t* userp_buftracker_RegisterNew(userp_buftracker_t *bt, int Len);
USERP_EXTERN void userp_buftracker_Unregister(userp_buftracker_t *bt, const uint8_t *Buffer);
USERP_EXTERN void userp_buftracker_AcquireRef(userp_buftracker_t *bt, const uint8_t *Pointer);
USERP_EXTERN void userp_buftracker_ReleaseRef(userp_buftracker_t *bt, const uint8_t *Pointer);
/** Calls "delete" on the buffer */
USERP_EXTERN void userp_buftracker_StdDeleteProc(const uint8_t* Buffer);
/** Calls "free" on the buffer */
USERP_EXTERN void userp_buftracker_StdFreeProc(const uint8_t* Buffer);

#ifdef USERP_CXX_IFACE
USERP_CLASSEXPORT class TBufTracker {
private:
	userp_buftracker_t* bt;
	TBufTracker(const TBufTracker&); // deny copying
	TBufTracker& operator = (const TBufTracker&); // deny assignment
public:
	typedef userp_buftracker_release_proc_t TReleaseProc;

	TBufTracker() {
		bt= NULL;
		bt= userp_buftracker_Create();
	}

	~TBufTracker() {
		if (bt) userp_buftracker_Destroy(bt);
	}

	inline bool IsTracking(const uint8_t* Pointer) const {
		return BufferOf(Pointer) != NULL;
	}

	inline const uint8_t* BufferOf(const uint8_t* Pointer) const {
		return userp_buftracker_BufferOf(const_cast<userp_buftracker_t*>(bt), Pointer);
	}

	inline void Register(const uint8_t* Buffer, int Len, TReleaseProc FreeProc) {
		userp_buftracker_Register(bt, Buffer, Len, FreeProc);
	}
	inline uint8_t* RegisterNew(int Len) {
		return userp_buftracker_RegisterNew(bt, Len);
	}
	inline void Unregister(const uint8_t *Buffer) {
		userp_buftracker_Unregister(bt, Buffer);
	}
	inline void Acquire(const uint8_t* Pointer) {
		userp_buftracker_AcquireRef(bt, Pointer);
	}
	inline void Release(const uint8_t* Pointer) {
		userp_buftracker_ReleaseRef(bt, Pointer);
	}
};
#endif

#ifdef USERP_CXX_IFACE
} // namespace
#endif

#endif
