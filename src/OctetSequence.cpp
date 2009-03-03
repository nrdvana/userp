#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include <assert.h>

#define USERP_CXX_IFACE
#include "OctetSequence.h"

#define FOREACH(i, start, limit) for (typeof(start) i= start, i##__limit__= limit; i != i##__limit__; ++i)

using namespace std;

namespace Userp {

//--------------------------  Exception Classes -------------------------------
#if 0
const char* IOctetSequence::EAddressOutOfBounds::what() const throw() {
	return "Address is outside of the available range of this data source";
}

const char* IOctetSequence::ECannotDereferenceLimit::what() const throw() {
	return "Attempt to dereference iterator at end() of data";
}

const char* IOctetSequence::ECannotIteratePastLimit::what() const throw() {
	return "Attempt to increment iterator at end() of data";
}

const char* TBufferTracker::EOverlappingBuffer::what() const throw() {
	return "Attempt to register a buffer in a memory range already registered";
}

const char* TBufferTracker::EBufferNotRegistered::what() const throw() {
	return "Attempt to acquire/release/unregister a buffer pointer which is not in a registered buffer";
}
#endif
//--------------------------- TOctetIter ----------------------------------

USERP_EXTERN void userp_octet_iter_Init(userp_octet_iter_t* iter, userp_octet_sequence_t* Owner, userp_octet_seq_addr_t StartAddr, userp_octet_iter_mode_t IterMode) {
	iter->Owner= Owner;
	iter->LimitAddr= StartAddr;
	iter->IterMode= IterMode;
	iter->Start= NULL;
	iter->Limit= NULL;
}

USERP_EXTERN userp_octet_seg_t userp_octet_iter_GetBufSeg(userp_octet_iter_t* iter) {
	return static_cast<TOctetIter*>(iter)->GetBufSeg();
}

USERP_EXTERN bool userp_octet_iter_NextBufSeg(userp_octet_iter_t* iter) {
	return static_cast<TOctetIter*>(iter)->NextBufSeg();
}

USERP_EXTERN userp_octet_seq_addr_t userp_octet_iter_GetAddr(const userp_octet_iter_t* iter) {
	return static_cast<const TOctetIter*>(iter)->GetAddr();
}

bool TOctetIter::NextBufSeg() {
	if (IterMode == imNoGrow)
		return false;
	TOctetSegment Buf;
	if (!static_cast<IOctetSequence*>(Owner)->NextBuf(&Buf, LimitAddr, IterMode))
		return false;
	Start= Buf.Start;
	Limit= Buf.Start+Buf.Len;
	LimitAddr+= Buf.Len;
	return Buf.Len != 0;
}

//--------------------------- IOctetSequence ----------------------------------

USERP_EXTERN void userp_octet_sequence_ReleaseUpTo(userp_octet_sequence_t *Sequence, userp_octet_seq_addr_t Offset) {
	static_cast<IOctetSequence*>(Sequence)->ReleaseUpTo(Offset);
}

USERP_EXTERN void userp_octet_sequence_AcquireMore(userp_octet_sequence_t *Sequence, bool ShouldBlock) {
	static_cast<IOctetSequence*>(Sequence)->AcquireMore(ShouldBlock);
}

USERP_EXTERN void userp_octet_sequence_Destroy(userp_octet_sequence_t *Sequence) {
	delete static_cast<IOctetSequence*>(Sequence);
}

USERP_EXTERN userp_octet_seq_addr_t userp_octet_sequence_getMinAddr(const userp_octet_sequence_t *Sequence) {
	return static_cast<const IOctetSequence*>(Sequence)->StartAddr();
}

USERP_EXTERN userp_octet_seq_addr_t userp_octet_sequence_getMaxAddr(const userp_octet_sequence_t *Sequence) {
	return static_cast<const IOctetSequence*>(Sequence)->EndAddr();
}

USERP_EXTERN bool userp_octet_sequence_getEOF(const userp_octet_sequence_t *Sequence) {
	return static_cast<const IOctetSequence*>(Sequence)->Eof();
}

USERP_EXTERN bool userp_octet_sequence_getERR(const userp_octet_sequence_t *Sequence) {
	return static_cast<const IOctetSequence*>(Sequence)->Err();
}

IOctetSequence::~IOctetSequence() {
}

//----------------------- TStreamedOctetSequence ------------------------------

USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateCustom(userp_octet_sequence_acquire_proc* AcquireProc, userp_octet_sequence_acquire_cleanup_proc* CleanupProc, void* passback) {
	return new TStreamedOctetSequence(AcquireProc, CleanupProc, passback);
}

struct TStreamedOctetSequence::Impl {
	struct TBufferSeg {
		TStreamedOctetSequence *Owner;
		const uint8_t *data;
		unsigned length_and_flags;

		inline TBufferSeg(const TBufferSeg& CopyFrom): Owner(CopyFrom.Owner), data(NULL), length_and_flags(0) {
			Assign(CopyFrom);
		}

		inline TBufferSeg(TStreamedOctetSequence *Owner, const uint8_t *data, int length, bool tracked)
			: Owner(Owner), data(data), length_and_flags((tracked?1:0) | (length<<1))
		{
			if (tracked) Owner->impl->KnownBuffers.Acquire(data);
		}

		inline ~TBufferSeg() { if (IsTracked()) Owner->impl->KnownBuffers.Release(data); }

		void Assign(const TBufferSeg& CopyFrom);

		inline TBufferSeg& operator = (const TBufferSeg& CopyFrom) { Assign(CopyFrom); return *this; }

		inline const uint8_t *Data() const { return data; }

		inline unsigned Length() const { return length_and_flags>>1; }

		inline bool IsTracked() const { return length_and_flags&1; }

		inline void Stretch(int AppendLen) { length_and_flags+= AppendLen<<1; }
	};

	typedef std::map<TAddr, TBufferSeg> TBufAddrMap;

	TBufTracker KnownBuffers;
	TAcquireProc *Acquire;
	void* AcquireCallbackData;
	TBufAddrMap BufSegments;

	~Impl() {
		// need to release all the buffers before destructing the BufTracker
		BufSegments.clear();
	}

	void AddSeg(TAddr addr, TStreamedOctetSequence *Owner, const uint8_t *data, int length, bool tracked) {
		BufSegments.insert(TBufAddrMap::value_type(addr, TBufferSeg(Owner, data, length, tracked)));
	}
};

void TStreamedOctetSequence::Impl::TBufferSeg::Assign(const TBufferSeg& CopyFrom) {
	if (IsTracked()) Owner->impl->KnownBuffers.Release(data);
	data= CopyFrom.data;
	length_and_flags= CopyFrom.length_and_flags;
	if (IsTracked()) Owner->impl->KnownBuffers.Acquire(data);
}

TStreamedOctetSequence::TStreamedOctetSequence(TAcquireProc acquire_proc, TAcquireCleanupProc acquire_cleanup, void *passback):
	impl(NULL), acquire_proc(acquire_proc), acquire_cleanup(acquire_cleanup),
	acquire_passback(acquire_passback)
{
	impl= new Impl();
}

TStreamedOctetSequence::TStreamedOctetSequence(TOctetSource *source):
	impl(NULL), acquire_proc(TOctetSource::StaticAcquire),
	acquire_cleanup(TOctetSource::StaticAcquireCleanup), acquire_passback(source)
{
	impl= new Impl();
}

TStreamedOctetSequence::~TStreamedOctetSequence() {
	if (impl) { delete impl; impl= NULL; }
	if (acquire_cleanup) { acquire_cleanup(acquire_passback); }
}

bool TStreamedOctetSequence::NextBuf(TSegment *Result, TAddr Addr, TOctetIterMode IterMode) {
	assert(Addr >= OctSeq_Min || Addr <= OctSeq_Max);
	if (Addr == OctSeq_Max)
		if (!AcquireMore(IterMode == imBlocking))
			return false;
	Impl::TBufAddrMap::iterator StartBuf= impl->BufSegments.upper_bound(Addr);
	--StartBuf;
	int Offset= Addr - StartBuf->first;
	Result->Start= StartBuf->second.Data() + Offset;
	Result->Len= StartBuf->second.Length() - Offset;
	return true;
}

void TStreamedOctetSequence::ReleaseUpTo(TAddr addr) {
	Impl::TBufAddrMap::iterator NewBegin= impl->BufSegments.upper_bound(addr);
	if (NewBegin == impl->BufSegments.begin())
		return; // already released up to that address
	--NewBegin; // now NewBegin is the buffer that contains addr
	OctSeq_Min= addr;
	impl->BufSegments.erase(impl->BufSegments.begin(), NewBegin);
}

bool TStreamedOctetSequence::AcquireMore(bool block) {
	const uint8_t *DataStart= NULL;
	int DataLen= 0;
	if (!acquire_proc(acquire_passback, impl->KnownBuffers, &DataStart, &DataLen, block)) {
		OctSeq_Eof= true;
		return false;
	}
	bool CanConcat= false;
	if (!impl->BufSegments.empty()) {
		// Can we concatenate the new buffer segment with the prev one?
		Impl::TBufferSeg &PrevRef= impl->BufSegments.rbegin()->second;
		CanConcat= PrevRef.Data()+PrevRef.Length() == DataStart
			&& impl->KnownBuffers.BufferOf(PrevRef.Data()) == impl->KnownBuffers.BufferOf(DataStart);
		if (CanConcat)
			PrevRef.Stretch(DataLen);
	}
	if (!CanConcat) {
		bool IsTracked= impl->KnownBuffers.IsTracking(DataStart);
		impl->AddSeg(OctSeq_Max, this, DataStart, DataLen, IsTracked);
	}
	OctSeq_Max+= DataLen;
	return true;
}

//-------------------------------- TOctetSource -------------------------------------

bool TOctetSource::StaticAcquire(void* CallbackData, userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool ShouldBlock) {
	TOctetSource* obj= (TOctetSource*) CallbackData;
	return obj->Acquire(BufTracker, DataStart, DataLen, ShouldBlock);
}

void TOctetSource::StaticAcquireCleanup(void* CallbackData) {
	delete (TOctetSource*) CallbackData;
}

//-------------------------------- TFDSource -------------------------------------

USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateFromFd(int fd) {
	return new TStreamedOctetSequence(new TFDSource(fd));
}

bool TFDSource::Acquire(userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block) {
	if (!Block)
		throw "FDSource::AcquireProc: nonblocking acquire is not supported (yet)";
	if (Buffer == NULL) {
		BufferLen= AllocSize;
		Buffer= userp_buftracker_RegisterNew(BufTracker, BufferLen);
		BufPos= 0;
	}
	int red;
	do
		red= read(fd, Buffer+BufPos, min(ReadLimit, BufferLen-BufPos));
	while (red < 0 && errno == EINTR);
	if (red < 0)
		return false;
	*DataStart= Buffer+BufPos;
	*DataLen= red;
	ReadLimit-= red;
	BufPos+= red;
	if (BufferLen - BufPos < 16) // just get a new buffer next time
		Buffer= NULL;
	return true;
}

#if 0
USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateFromFile(FILE* file) {
	return new TStreamedOctetSequence(new TFileSource(file));
}
#endif
} // namespace
