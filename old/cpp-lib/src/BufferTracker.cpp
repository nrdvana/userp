#define USERP_CXX_IFACE
#include "BufferTracker.h"

#include <stdio.h>
#include <map>
#define FOREACH(i, start, limit) for (typeof(start) i= start, i##__limit__= limit; i != i##__limit__; ++i)

namespace Userp {

using namespace std;

struct TBufTracker::Impl {
	struct TBufferInfo {
		int RefCnt;
		int Length;
		userp_buftracker_release_proc_t FreeProc;
		TBufferInfo(int Length, userp_buftracker_release_proc_t FreeProc): RefCnt(0), Length(Length), FreeProc(FreeProc) {}
	};
	typedef std::map<const uint8_t*, TBufferInfo> TBufInfMap;
	TBufInfMap BufferInfoMap;

	bool Find(const uint8_t* Pointer, TBufInfMap::iterator *Result);

	void Insert(const uint8_t *Buffer, int Len, userp_buftracker_release_proc_t FreeProc) {
		BufferInfoMap.insert(TBufInfMap::value_type(Buffer, TBufferInfo(Len, FreeProc)));
	}

	~userp_buftracker();
	
	void PrintBuffers(FILE* dest);
};

TBufTracker::Impl::~Impl() {
	if (BufferInfoMap.size()) {
		fprintf(stderr, "All buffers must be released before freeing the buffer tracker!\n");
		PrintBuffers(stderr);
		abort();
	}
}

bool TBufTracker::Impl::Find(const uint8_t* Pointer, TBufInfMap::iterator *Result) {
	TBufInfMap::iterator Nearest= BufferInfoMap.lower_bound(Pointer);
	// if nearest entry is not equal to pointer, then it is greater than pointer
	if (Nearest == BufferInfoMap.end() || Nearest->first != Pointer) {
		// need to seek to previous buffer entry
		if (Nearest == BufferInfoMap.begin())
			return false;
		--Nearest;
		int Diff= Pointer - Nearest->first;
		if (Diff < 0 || Diff >= Nearest->second.Length)
		return false;
	}
	if (Result)
		*Result= Nearest;
	return true;
}

void TBufTracker::Impl::PrintBuffers(FILE* dest) {
	fprintf(stderr, "Registered buffers:\n");
	FOREACH(entry, BufferInfoMap.begin(), BufferInfoMap.end())
		fprintf(stderr, "\t%p + %d  (%d refs)\n", entry->first, entry->second.Length, entry->second.RefCnt);
}

const uint8_t* BufferOf(const uint8_t* Pointer) const {
}

void Register(const uint8_t* Buffer, int Len, TReleaseProc FreeProc) {
}

uint8_t* RegisterNew(int Len) {
}

void Unregister(const uint8_t *Buffer) {
}

void Acquire(const uint8_t* Pointer) {
}

void Release(const uint8_t* Pointer) {
}

extern "C" {

userp_buftracker_t* userp_buftracker_Create() {
	return new userp_buftracker();
}

void userp_buftracker_Destroy(userp_buftracker_t *bt) {
	delete bt;
}

void userp_buftracker_FreeAllBuffers(userp_buftracker_t *bt) {
	FOREACH(entry, bt->BufferInfoMap.begin(), bt->BufferInfoMap.end())
		if (entry->second.FreeProc)
			entry->second.FreeProc(entry->first);
	bt->BufferInfoMap.clear();
}

const uint8_t *userp_buftracker_BufferOf(userp_buftracker_t *bt, const uint8_t *Pointer) {
	userp_buftracker_t::TBufInfMap::iterator Found;
	if (bt->Find(Pointer, &Found))
		return Found->first;
	else
		return NULL;
}

void userp_buftracker_Register(userp_buftracker_t *bt, const uint8_t *Buffer, int Len, userp_buftracker_release_proc_t FreeProc) {
	userp_buftracker_t::TBufInfMap::iterator Found;
	if (bt->Find(Buffer, &Found)) {
		const char* msg;
		if (Found->first == Buffer && Found->second.Length == Len)
			// duplicate registry of the buffer.
			msg= "Attempt to re-register an existing buffer: %p,%d\n";
		else
			// conflict of addresses.
			msg= "Attempt to register a buffer (%0,%d) that overlaps an existing buffer\n";
		fprintf(stderr, msg, Buffer, Len);
		bt->PrintBuffers(stderr);
		abort();
	}
	bt->Insert(Buffer, Len, FreeProc);
}

uint8_t* userp_buftracker_RegisterNew(userp_buftracker_t *bt, int Len) {
	uint8_t* Result= new uint8_t[Len];
	userp_buftracker_Register(bt, Result, Len, userp_buftracker_StdDeleteProc);
	return Result;
}

void userp_buftracker_Unregister(userp_buftracker_t *bt, const uint8_t *Buffer) {
	if (!bt->BufferInfoMap.erase(Buffer)) {
		fprintf(stderr, "Unregister: Buffer was not registered: %p\n", Buffer);
		bt->PrintBuffers(stderr);
		abort();
	}
}

void userp_buftracker_AcquireRef(userp_buftracker_t *bt, const uint8_t *Pointer) {
	userp_buftracker_t::TBufInfMap::iterator Found;
	if (!bt->Find(Pointer, &Found)) {
		fprintf(stderr, "AcquireRef: No buffer contains %p\n", Pointer);
		bt->PrintBuffers(stderr);
		abort();
	}
	Found->second.RefCnt++;
}

void userp_buftracker_ReleaseRef(userp_buftracker_t *bt, const uint8_t *Pointer) {
	userp_buftracker_t::TBufInfMap::iterator Found;
	if (!bt->Find(Pointer, &Found)) {
		fprintf(stderr, "ReleaseRef: No buffer contains %p\n", Pointer);
		bt->PrintBuffers(stderr);
		abort();
	}
	if (!--Found->second.RefCnt) {
		const uint8_t* Buffer= Found->first;
		userp_buftracker_release_proc_t FreeProc= Found->second.FreeProc;
		bt->BufferInfoMap.erase(Found);
		// Run this last, because it could have all kinds of side effects
		//  even as crazy as deleting this current BufTracker instance.
		FreeProc(Buffer);
	}
}

void userp_buftracker_StdDeleteProc(const uint8_t* Buffer) {
	delete[] Buffer;
}

void userp_buftracker_StdFreeProc(const uint8_t* Buffer) {
	free(const_cast<uint8_t*>(Buffer));
}

} // extern "C"

} //namespace
