#ifndef USERP_OCTETSEQUENCE_H
#define USERP_OCTETSEQUENCE_H

#include <stdint.h>
#include "HeaderDefs.h"
#include "Misc.h"

#ifdef USERP_CXX_IFACE
#include <exception>
namespace Userp {
#endif

// "Octet Sequence" class
struct userp_octet_sequence;

// type used for addresses within an octet sequence
typedef uint64_t userp_octet_seq_addr_t;

/* A block-segment of octets in the sequence.
 * For C++, we subclass from TArray, to get lots of handy methods.
 */
#ifdef USERP_CXX_IFACE
typedef TArray<const uint8_t> TOctetSegment;
typedef TOctetSegment userp_octet_seg_t;
#else
typedef struct userp_octet_seg {
	const uint8_t* Start;
	int Len;
} userp_octet_seg_t;
#endif

/* The mode for an octet sequence iterator */
#ifdef USERP_CXX_IFACE
enum TOctetIterMode { imNoGrow= 1, imBlocking= 2, imNonblocking= 3 };
typedef TOctetIterMode userp_octet_iter_mode_t;
#else
typedef int userp_octet_iter_mode_t;
#define USERP_OCTET_ITER_MODE_NOGROW 1
#define USERP_OCTET_ITER_MODE_BLOCKING 2
#define USERP_OCTET_ITER_MODE_NONBLOCKING 3
#endif

/* A struct describing an iterator for an octet sequence */
struct userp_octet_iter_t {
	userp_octet_seq_t *Owner;
	userp_octet_seq_addr_t LimitAddr;
	const uint8_t *Start, *Limit;
	userp_octet_iter_mode_t IterMode;
};
USERP_EXTERN userp_octet_iter_Init(struct octet_iter_t*, octet_sequence_t* Owner, TAddr StartAddr, TOctetIterateMode IterMode);
USERP_EXTERN userp_octet_seg_t userp_octet_iter_GetBufSeg(struct octet_iter_t*);
USERP_EXTERN bool userp_octet_iter_NextBufSeg(struct octet_iter_t*);
USERP_EXTERN userp_octet_seq_addr_t userp_octet_iter_GetAddr(struct octet_iter_t*);
#define USERP_OCTET_ITER_EOF(iter) ( iter->Start == Limit && !userp_octet_iter_NextBufSeg(iter) )
#ifdef USERP_CXX_IFACE
USERP_CLASSEXPORT class TOctetIter: protected userp_octet_iter_t {
public:
	typedef userp_octet_seq_addr_t TAddr;

	struct ECannotDereferenceLimit: public std::exception {
		virtual const char* what() const throw();
	};

	struct ECannotIteratePastLimit: public std::exception {
		virtual const char* what() const throw();
	};

	/** Constructor
	 * Values are stored.  No other actions are taken.
	 * The iterator will not point to real data until NextBufSeg is called.
	 */
	TOctetIter(userp_octet_seq_t *Owner, TAddr StartAddr, TOctetIterateMode IterMode)
		{ this->Owner= Owner, this->LimitAddr= StartAddr, this->Start= NULL, this->Limit= NULL, this->IterMode= IterMode; }

	/** Get the sequence address of the current position of the iterator */
	inline TAddr GetAddr() const {
		return LimitAddr - (Limit-Start);
	}

	/** Get the segment this iterator currently points to.
	 * This segment could potentially be empty, if we are at EOF.
	 */
	inline TOctetSegment GetBufSeg() {
		if (Start == Limit)
			NextBufSeg();
		return TOctetSegment(Start, Limit-Start);
	}

	/** Get whether this iterator has reached the end of the sequence.
	 * Note that the definition of EOF differs depending on IterMode
	 */
	inline bool Eof() {
		return Start == Limit && !NextBufSeg();
	}

	/** Get the next buffer segment for this iterator.
	 * @return true, if there is a new segment, and false if there is not
	 */
	inline bool NextBufSeg() { return userp_octet_iter_NextBufSeg(this); }

	/** Get the current octet this iterator points to. */
	inline uint8_t operator * () {
		if (Eof())
			throw ECannotDereferenceLimit();
		return *Start;
	}

	/** Increment the iterator.
	 * Throws an exception if the iterator was already at EOF.
	 */
	inline TOctetIter& operator ++ () {
		if (Eof())
			throw ECannotIteratePastLimit();
		Start++;
		return *this;
	}

	/** Check for iterator equality.
	 * The only thing that matters is the address.
	 */
	inline bool operator == (const TOctetIter &Peer) const {
		return GetAddr() == Peer.GetAddr() && Owner == Peer.Owner;
	}

	/** Is the iterator not at EOF yet?
	 * I just personally like boolean evaluation of objects.  Its handy in loops.
	 */
	inline operator bool () { return !Eof(); }
};
#endif

struct octet_sequence_t {
	int OctetSequence_Flags;
	octet_sequence_addr_t OctetSequence_Min;
	octet_sequence_addr_t OctetSequence_Max;
};
#define OCTET_SEQUENCE_FLAG_EOF 1
#define OCTET_SEQUENCE_FLAG_ERR 2

typedef bool octet_sequence_acquire_proc(void* CallbackData, struct buffer_tracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool ShouldBlock);

OCTETSEQUENCE_H_EXTERN struct octet_sequence_t* octet_sequence_Create(octet_sequence_acquire_proc* AcquireProc);
OCTETSEQUENCE_H_EXTERN struct octet_sequence_t* octet_sequence_Create(int FileDescriptor);
OCTETSEQUENCE_H_EXTERN struct octet_sequence_t* octet_sequence_Create(FILE* file);
OCTETSEQUENCE_H_EXTERN bool octet_sequence_GetIter(struct octet_sequence_t *Sequence, octet_sequence_addr_t Offset, iterate_mode_t Mode, struct octet_iter_t* Result);
OCTETSEQUENCE_H_EXTERN void octet_sequence_ReleaseUpTo(struct octet_sequence_t *Sequence, octet_sequence_addr_t Offset);
OCTETSEQUENCE_H_EXTERN void octet_sequence_AcquireMore(struct octet_sequence_t *Sequence, bool ShouldBlock);
OCTETSEQUENCE_H_EXTERN void octet_sequence_Destroy(struct octet_sequence_t *Sequence);

#ifdef USERP_CXX_IFACE
class IOctetSequence {
public:
	typedef octet_sequence_addr_t TAddr;
	typedef TOctetSegment TSegment;

	struct EAddressOutOfBounds: public std::exception {
		TAddr errAt, start, limit;
		EAddressOutOfBounds(TAddr errAt, TAddr start, TAddr limit)
			: errAt(errAt), start(start), limit(limit) {}
		virtual const char* what() const throw();
	};

	typedef TOctetIter iterator;
	typedef TOctetIter const_iterator;
	typedef const uint8_t value_type;

protected:
	void SetEof() { OctetSequence_Flags|= OCTET_SEQUENCE_FLAG_EOF; }
	void SetErr() { OctetSequence_Flags|= OCTET_SEQUENCE_FLAG_ERR; }
	virtual bool NextBuf(TSegment *Result, TAddr Addr, TIterateMode IterMode)= 0;
public:
	IOctetSequence() { OctetSequence_Flags= 0, OctetSequence_Min= 0, OctetSequence_Max= 0; }
	virtual ~IOctetSequence() {}
	virtual TOctetIter GetIter(TAddr offset, TIterateMode mode)= 0;
	virtual void ReleaseUpTo(TAddr offset)= 0;
	virtual bool AcquireMore(bool block)= 0;
	inline bool AcquireMore() { return AcquireMore(true); }

	inline TAddr StartAddr() { return OctetSequence_Min; }
	inline TAddr EndAddr() { return OctetSequence_Max; }
	inline iterator begin() { return GetIter(OctetSequence_Min, imNoGrow); }
	inline iterator end() { return GetIter(OctetSequence_Max, imNoGrow); }
	inline bool Eof() { return OctetSequence_Flags&1; }
	inline bool Err() { return OctetSequence_Flags&2; }
};

class TStreamedOctetSequence: public IOctetSequence {
	class Impl;
	Impl *impl; // implementation, which we want to hide fomr the header file
	friend class Impl;
public:
	typedef bool TAcquireFunc(void* CallbackData, TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block);

protected:
	virtual bool NextBuf(TData *Result, TAddr Addr, TIterateMode IterMode);
	virtual bool Acquire_Internal(TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block)= 0;

public:
	TStreamedOctetSequence();
	virtual ~TStreamedOctetSequence();

	virtual TDataIter GetIter(TAddr addr, TIterateMode mode);
	virtual void ReleaseUpTo(TAddr addr);
	virtual bool AcquireMore(bool block);
};
/*
	class FDSource {
		int fd;
		uint8_t *Buffer;
		unsigned BufferLen;
		int AllocSize;
		int BufPos;
		unsigned ReadLimit;
		bool Acquire(TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block);
	public:
		FDSource(int fd, unsigned ReadLimit= 0U-1U): fd(fd), Buffer(NULL),
			AllocSize(256), ReadLimit(ReadLimit) {}
		void SetAllocSize(int NewSize) { AllocSize= NewSize; }
		static bool AcquireProc(void* CallbackData, TBufferTracker *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block);
	};
};
*/
#ifdef USERP_CXX_IFACE
} // namespace
#else
  #ifdef CPLUSPLUS
  #undef OCTETSEQUENCE_H_EXTERN
  #endif
#endif

#endif // include-guard
