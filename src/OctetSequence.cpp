#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>
#include <vector>
#include <list>
#include <algorithm>
#include "OctetSequence.h"

#define FOREACH(i, start, limit) for (typeof(start) i= start, i##__limit__= limit; i != i##__limit__; ++i)

using namespace std;

namespace Userp {

//--------------------------  Exception Classes -------------------------------

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

//--------------------------- IOctetSequence ----------------------------------

bool IOctetsequence::TOctetIter::NextBufSeg() {
	if (IterMode == imNoGrow)
		return false;
	TData Buf;
	if (!Owner->NextBuf(&Buf, LimitAddr, IterMode))
		return false;
	Start= Buf.Start;
	Limit= Buf.Start+Buf.Len;
	LimitAddr+= Buf.Len;
	return Buf.Len != 0;
}

//----------------------- TStreamedOctetSequence ------------------------------

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
			if (tracked) Owner->KnownBuffers.Acquire(data);
		}

		inline ~TBufferSeg() { if (IsTracked()) Owner->KnownBuffers.Release(data); }

		void Assign(const TBufferSeg& CopyFrom);

		inline TBufferSeg& operator = (const TBufferSeg& CopyFrom) { Assign(CopyFrom); return *this; }

		inline const uint8_t *Data() const { return data; }

		inline unsigned Length() const { return length_and_flags>>1; }

		inline bool IsTracked() const { return length_and_flags&1; }

		inline void Stretch(int AppendLen) { length_and_flags+= AppendLen<<1; }
	};

	typedef std::map<TAddr, TBufferSeg> TBufAddrMap;

	TBufferTracker KnownBuffers;
	TAcquireFunc *Acquire;
	void* AcquireCallbackData;
	TBufAddrMap BufSegments;
};

void TStreamedOctetSequence::Impl::TBufferSeg::Assign(const TBufferSeg& CopyFrom) {
	if (IsTracked()) Owner->KnownBuffers.Release(data);
	data= CopyFrom.data;
	length_and_flags= CopyFrom.length_and_flags;
	if (IsTracked()) Owner->KnownBuffers.Acquire(data);
}

TStreamedOctetSequence::TStreamedOctetSequence() {
	impl= NULL;
	impl= new Impl();
}

TStreamedOctetSequence::~TStreamedOctetSequence() {
	if (impl) delete impl, impl= NULL;
}

bool TStreamedOctetSequence::NextBuf(TData *Result, TAddr Addr, TIterateMode IterMode) {
	if (Addr < DataSource_Min || Addr > DataSource_Max)
		throw EAddressOutOfBounds(Addr, DataSource_Min, DataSource_Max);
	if (Addr == DataSource_Max)
		if (!AcquireMore(IterMode == imBlocking))
			return false;
	TBufAddrMap::iterator StartBuf= BufSegments.upper_bound(Addr);
	--StartBuf;
	int Offset= Addr-StartBuf->first;
	Result->Start= StartBuf->second.Data() + Offset;
	Result->Len= StartBuf->second.Length() - Offset;
	return true;
}

IOctetSequence::TOctetIter TStreamedOctetSequence::GetIter(TAddr addr, TIterateMode mode) {
	if (addr < DataSource_Min || addr > DataSource_Max)
		throw EAddressOutOfBounds(addr, DataSource_Min, DataSource_Max);
	if (BufSegments.size() == 0) // for the case of no buffers at all
		return TDataIter(this, addr, mode);
	TBufAddrMap::iterator StartBuf= BufSegments.upper_bound(addr);
	--StartBuf;
	return TDataIter(this, addr - StartBuf->first, mode);
}

void TStreamedOctetSequence::ReleaseUpTo(TAddr addr) {
	TBufAddrMap::iterator NewBegin= BufSegments.upper_bound(addr);
	if (NewBegin == BufSegments.begin())
		return; // already released up to that address
	--NewBegin; // now NewBegin is the buffer that contains addr
	DataSource_Min= addr;
	BufSegments.erase(BufSegments.begin(), NewBegin);
}

bool TStreamedOctetSequence::AcquireMore(bool block) {
	const uint8_t *DataStart= NULL;
	int DataLen= 0;
	if (!Acquire(AcquireCallbackData, &KnownBuffers, &DataStart, &DataLen, block)) {
		SetEof();
		return false;
	}
	bool CanConcat= false;
	if (!BufSegments.empty()) {
		// Can we concatenate the new buffer segment with the prev one?
		TBufferSeg &PrevRef= BufSegments.rend()->second;
		CanConcat= PrevRef.Data()+PrevRef.Length() == DataStart
			&& KnownBuffers.BufferOf(PrevRef.Data()) == KnownBuffers.BufferOf(DataStart);
		if (CanConcat)
			PrevRef.Stretch(DataLen);
	}
	if (!CanConcat) {
		bool IsTracked= KnownBuffers.IsTracking(DataStart);
		BufSegments.insert(TBufAddrMap::value_type(DataSource_Max, TBufferSeg(this, DataStart, DataLen, IsTracked)));
	}
	DataSource_Max+= DataLen;
	return true;
}

bool TStreamedOctetSequence::FDSource::AcquireProc(void* CallbackData, TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block) {
	return ((FDSource*) CallbackData)->Acquire(BufTracker, DataStart, DataLen, Block);
}

bool TStreamedOctetSequence::FDSource::Acquire(TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block) {
	if (!Block)
		throw "FDSource::AcquireProc: nonblocking acquire is not supported (yet)";
	if (Buffer == NULL) {
		BufferLen= AllocSize;
		Buffer= BufTracker->RegisterNew(BufferLen);
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

} // namespace
