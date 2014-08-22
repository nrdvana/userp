#ifndef USERP_OCTETSEQUENCE_H
#define USERP_OCTETSEQUENCE_H

#include <stdint.h>
#include "HeaderDefs.h"
#include "Misc.h"
#include "BufferTracker.h"

#ifdef USERP_CXX_IFACE
#include <exception>
namespace Userp {
#endif

// "Octet Sequence" class
struct userp_octet_sequence;
typedef struct userp_octet_sequence userp_octet_sequence_t;

// type used for addresses within an octet sequence
typedef uint64_t userp_octet_seq_addr_t;

/* A block-segment of octets in the sequence.
 * For C++, we subclass from TArray, to get lots of handy methods.
 */
#ifdef USERP_CXX_IFACE
typedef TArray<const uint8_t> TOctetSegment;
typedef TOctetSegment userp_octet_seg_t;
#else
typedef struct const_byte_array userp_octet_seg_t;
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
typedef struct userp_octet_iter {
	userp_octet_sequence_t *Owner;
	userp_octet_seq_addr_t LimitAddr;
	const uint8_t *Start, *Limit;
	userp_octet_iter_mode_t IterMode;
} userp_octet_iter_t;

USERP_EXTERN void userp_octet_iter_Init(userp_octet_iter_t*, userp_octet_sequence_t* Owner, userp_octet_seq_addr_t StartAddr, userp_octet_iter_mode_t IterMode);
USERP_EXTERN userp_octet_seg_t userp_octet_iter_GetBufSeg(userp_octet_iter_t*);
USERP_EXTERN bool userp_octet_iter_NextBufSeg(userp_octet_iter_t*);
USERP_EXTERN userp_octet_seq_addr_t userp_octet_iter_GetAddr(const userp_octet_iter_t*);
#define USERP_OCTET_ITER_EOF(iter) ( iter->Start == Limit && !userp_octet_iter_NextBufSeg(iter) )
#ifdef USERP_CXX_IFACE
USERP_CLASSEXPORT class TOctetIter: public userp_octet_iter_t {
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
	TOctetIter(userp_octet_sequence_t *Owner, TAddr StartAddr, TOctetIterMode IterMode)
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
	inline bool NextBufSeg();

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

typedef struct userp_octet_sequence {
} userp_octet_sequence_t;
#define OCTET_SEQUENCE_FLAG_EOF 1
#define OCTET_SEQUENCE_FLAG_ERR 2

typedef bool userp_octet_sequence_acquire_proc(void* CallbackData, userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool ShouldBlock);
typedef void userp_octet_sequence_acquire_cleanup_proc(void* CallbackData);

USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateCustom(userp_octet_sequence_acquire_proc* AcquireProc, userp_octet_sequence_acquire_cleanup_proc* CleanupProc, void* passback);
USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateFromFd(int FileDescriptor);
USERP_EXTERN userp_octet_sequence_t* userp_octet_sequence_CreateFromFile(FILE* file);
USERP_EXTERN userp_octet_seq_addr_t userp_octet_sequence_getMinAddr(const userp_octet_sequence_t *Sequence);
USERP_EXTERN userp_octet_seq_addr_t userp_octet_sequence_getMaxAddr(const userp_octet_sequence_t *Sequence);
USERP_EXTERN bool userp_octet_sequence_getEOF(const userp_octet_sequence_t *Sequence);
USERP_EXTERN bool userp_octet_sequence_getERR(const userp_octet_sequence_t *Sequence);
USERP_EXTERN void userp_octet_sequence_ReleaseUpTo(userp_octet_sequence_t *Sequence, userp_octet_seq_addr_t Offset);
USERP_EXTERN void userp_octet_sequence_AcquireMore(userp_octet_sequence_t *Sequence, bool ShouldBlock);
USERP_EXTERN void userp_octet_sequence_Destroy(userp_octet_sequence_t *Sequence);

#ifdef USERP_CXX_IFACE
class USERP_CLASSEXPORT IOctetSequence: public userp_octet_sequence_t {
public:
	typedef userp_octet_seq_addr_t TAddr;
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
	int
	  OctSeq_Eof: 1,
	  OctSeq_Err: 1;
	TAddr OctSeq_Min;
	TAddr OctSeq_Max;

	virtual bool NextBuf(TSegment *Result, TAddr Addr, TOctetIterMode IterMode)= 0;
	friend class TOctetIter;
public:
	IOctetSequence(): OctSeq_Eof(0), OctSeq_Err(0), OctSeq_Min(0), OctSeq_Max(0) {}
	virtual ~IOctetSequence();

	inline TOctetIter GetIter(TAddr addr, TOctetIterMode mode) {
		if (addr < OctSeq_Min || addr > OctSeq_Max)
			throw EAddressOutOfBounds(addr, OctSeq_Min, OctSeq_Max);
		return TOctetIter(this, addr, mode);
	}

	virtual void ReleaseUpTo(TAddr offset)= 0;
	virtual bool AcquireMore(bool block= true)= 0;

	inline TAddr StartAddr() const { return OctSeq_Min; }
	inline TAddr EndAddr() const { return OctSeq_Max; }
	inline iterator begin() { return GetIter(OctSeq_Min, imNoGrow); }
	inline iterator end() { return GetIter(OctSeq_Max, imNoGrow); }
	inline bool Eof() const { return OctSeq_Eof; }
	inline bool Err() const { return OctSeq_Err; }
};

class USERP_CLASSEXPORT TOctetSource {
public:
	static bool StaticAcquire(void* CallbackData, userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool ShouldBlock);
	static void StaticAcquireCleanup(void* CallbackData);

	TOctetSource() {}
	virtual ~TOctetSource() {}
	virtual bool Acquire(userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool ShouldBlock)= 0;
};

class USERP_CLASSEXPORT TStreamedOctetSequence: public IOctetSequence {
	class Impl;
	Impl *impl; // implementation, which we want to hide from the header file
	friend class Impl;
protected:
	typedef userp_octet_sequence_acquire_proc TAcquireProc;
	typedef userp_octet_sequence_acquire_cleanup_proc TAcquireCleanupProc;

	TAcquireProc *acquire_proc;
	TAcquireCleanupProc *acquire_cleanup;
	void *acquire_passback;

	virtual bool NextBuf(TSegment *Result, TAddr Addr, TOctetIterMode IterMode);
public:
	TStreamedOctetSequence(TAcquireProc *acquire_proc, TAcquireCleanupProc *acquire_cleanup, void* passback);
	TStreamedOctetSequence(TOctetSource *source);
	virtual ~TStreamedOctetSequence();

	virtual void ReleaseUpTo(TAddr addr);
	virtual bool AcquireMore(bool block);
};

class USERP_CLASSEXPORT TFDSource: public TOctetSource {
	int fd;
	uint8_t *Buffer;
	unsigned BufferLen;
	int AllocSize;
	int BufPos;
	unsigned ReadLimit;
public:
	TFDSource(int fd, unsigned ReadLimit= 0U-1U): fd(fd), Buffer(NULL),
		AllocSize(256), ReadLimit(ReadLimit) {}
	virtual ~TFDSource();
	void SetAllocSize(int NewSize) { AllocSize= NewSize; }
	bool Acquire(userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block);
};

#if 0
class USERP_CLASSEXPORT TFileSource: public TOctetSource {
	bool Acquire(userp_buftracker_t *BufTracker, const uint8_t **DataStart, int *DataLen, bool Block);
};
#endif
#endif

#ifdef USERP_CXX_IFACE
} // namespace
#else
  #ifdef CPLUSPLUS
  #undef OCTETSEQUENCE_H_EXTERN
  #endif
#endif

#endif // include-guard
