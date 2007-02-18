package com.silverdirk.userp;

import java.util.LinkedList;
import java.util.Iterator;
import java.io.IOException;
import java.lang.reflect.Array;
import java.math.BigInteger;
import java.util.Date;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Reader and Writer implementation</p>
 * <p>Description: Objects of this class are used to perform the reading and writing and inspection of values.</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
class ReaderWriterImpl implements ReaderImpl {
	public void inspectActualType(UserpReader reader) throws IOException {
		throw new RuntimeException("This type has no derivative.");
	}
	public void inspectElements(UserpReader reader) throws IOException {
		throw new RuntimeException("This type has no elements.");
	}
	public void nextElement(UserpReader reader) throws IOException {
		throw new RuntimeException("This type has no elements.");
	}
	public void endElements(UserpReader reader) throws IOException {
		throw new RuntimeException("This type has no elements.");
	}
	public int loadValue(UserpReader reader) throws IOException {
		throw new UnsupportedOperationException();
	}
	public void skipValue(UserpReader reader) throws IOException {
		loadValue(reader);
	}
}

class AnyImpl extends ReaderWriterImpl {
	public void inspectActualType(UserpReader reader) throws IOException {
		reader.elemType= reader.readType(reader);
		reader.scalarIsLoaded= false;
	}

	public int loadValue(UserpReader reader) throws IOException {
		reader.inspectDerivative();
		reader.valueObj= new TypedData(reader.getType(), reader.readValue());
		return reader.NATIVETYPE_OBJECT;
	}

	static final AnyImpl INSTANCE= new AnyImpl();
}

class WholeImpl extends ReaderWriterImpl {
	public int loadValue(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded)
			reader.readVarQty();
		else
			reader.scalarIsLoaded= false; // we're going to consume it
		if (reader.scalar != null) {
			reader.valueObj= reader.scalar;
			return UserpReader.NATIVETYPE_OBJECT;
		}
		else {
			reader.value64= reader.scalar64;
			return (reader.value64>>>31) != 0? UserpReader.NATIVETYPE_LONG : UserpReader.NATIVETYPE_INT;
		}
	}

	static final WholeImpl INSTANCE= new WholeImpl();
}

class NegWholeImpl extends WholeImpl {
	public int loadValue(UserpReader reader) throws IOException {
		int valType= super.loadValue(reader);
		if (valType == UserpReader.NATIVETYPE_OBJECT) {
			reader.valueObj= ((BigInteger)reader.valueObj).negate();
			// special case, for the one number who's scalar doesn't fit in 63
			//  bits but whose negation does
			if (reader.valueObj.equals(LONG_MIN)) {
				reader.value64= Long.MIN_VALUE;
				valType= UserpReader.NATIVETYPE_LONG;
			}
		}
		else {
			reader.value64= -reader.value64;
			// special case, for the one number who's scalar doesn't fit in 31
			//  bits but whose negation does
			if ((reader.value64>>31) == -1L)
				valType= UserpReader.NATIVETYPE_INT;
		}
		return valType;
	}

	static final NegWholeImpl INSTANCE= new NegWholeImpl();
	static final BigInteger LONG_MIN= BigInteger.valueOf(Long.MIN_VALUE);
}

class TypeImpl extends ReaderWriterImpl {
	public int loadValue(UserpReader reader) throws IOException {
		reader.valueObj= reader.readType(reader);
		return UserpReader.NATIVETYPE_OBJECT;
	}

	static final TypeImpl INSTANCE= new TypeImpl();
}

class EnumImpl extends ReaderWriterImpl {
	public int loadValue(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded)
			reader.readQty(reader.elemType.getScalarBitLen());
		else
			reader.scalarIsLoaded= false; // we're going to consume it
		reader.value64= reader.scalarAsLong();
		return reader.NATIVETYPE_INT;
	}

	static final EnumImpl INSTANCE= new EnumImpl();
}

class RangeImpl_Long extends ReaderWriterImpl {
	public void inspectActualType(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded) {
			reader.readQty(reader.elemType.getScalarBitLen());
			reader.scalarIsLoaded= true;
		}
		long ofs= reader.scalarAsLong();
		RangeType.RangeDef def= ((RangeType)reader.getType()).rangeDef;
		reader.scalar64= def.invert? def.max_l - ofs : def.min_l + ofs;
		reader.scalar= null;
		reader.elemType= def.getBaseType();
	}

	public int loadValue(UserpReader reader) throws IOException {
		reader.inspectDerivative();
		return reader.elemType.impl.loadValue(reader);
	}

	static final RangeImpl_Long INSTANCE= new RangeImpl_Long();
}

class RangeImpl_InfRange extends RangeImpl_Long {
	public void inspectActualType(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded) {
			reader.readVarQty();
			reader.scalarIsLoaded= true;
		}
		RangeType.RangeDef def= ((RangeType)reader.getType()).rangeDef;
		if (reader.scalar == null)
			reader.scalar64+= def.min_l;
		else
			reader.scalar= def.min.add(reader.scalar);
		reader.elemType= def.getBaseType();
	}

	static final RangeImpl_InfRange INSTANCE= new RangeImpl_InfRange();
}

class RangeImpl_BigInt extends RangeImpl_Long {
	public void inspectActualType(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded) {
			reader.readQty(reader.elemType.getScalarBitLen());
			reader.scalarIsLoaded= true;
		}
		RangeType.RangeDef def= ((RangeType)reader.getType()).rangeDef;
		BigInteger ofs= reader.scalarAsBigInt();
		reader.scalar= def.invert? def.max.subtract(ofs) : def.min.add(ofs);
		reader.elemType= def.getBaseType();
	}

	static final RangeImpl_BigInt INSTANCE= new RangeImpl_BigInt();
}

class UnionImpl_Long extends ReaderWriterImpl {
	public void inspectActualType(UserpReader reader) throws IOException {
		if (!reader.scalarIsLoaded)
			reader.readQty(reader.elemType.getScalarBitLen());
		else if (reader.scalar != null) {
			reader.scalar64= reader.scalarAsLong();
			reader.scalar= null;
		}
		performInspect(reader.scalar64, reader);
	}

	public int loadValue(UserpReader reader) throws IOException {
		reader.inspectDerivative();
		reader.valueObj= new TypedData(reader.getType(), reader.readValue());
		return reader.NATIVETYPE_OBJECT;
	}

	protected static void performInspect(long val, UserpReader reader) {
		UnionType.UnionDef def= ((UnionType)reader.elemType).unionDef;
		int typeIdx= def.findRangeIdxOf(def.offsets_l, val);
		reader.elemType= def.getTypeRef(typeIdx);
		reader.scalarIsLoaded= def.inlined[typeIdx];
		if (reader.scalarIsLoaded)
			reader.scalar64= val - def.offsets_l[typeIdx];
	}

	protected static void performInspect(BigInteger val, UserpReader reader) {
		UnionType.UnionDef def= ((UnionType)reader.elemType).unionDef;
		int typeIdx= def.findRangeIdxOf(def.offsets, val);
		reader.elemType= def.getTypeRef(typeIdx);
		reader.scalarIsLoaded= def.inlined[typeIdx];
		if (reader.scalarIsLoaded)
			reader.scalar= val.subtract(def.offsets[typeIdx]);
	}

	static final UnionImpl_Long INSTANCE= new UnionImpl_Long();
}

class UnionImpl_BigInt extends UnionImpl_Long {
	public void inspectActualType(UserpReader reader) throws IOException {
		UnionType.UnionDef def= ((UnionType)reader.getType()).unionDef;
		if (!reader.scalarIsLoaded)
			reader.readQty(reader.elemType.getScalarBitLen());
		if (reader.scalar == null) {
			if (reader.scalar64 != Long.MAX_VALUE)
				performInspect(reader.scalar64, reader);
			else
				performInspect(BigInteger.valueOf(reader.scalar64), reader);
		}
		else
			performInspect(reader.scalar, reader);
	}

	static final UnionImpl_BigInt INSTANCE= new UnionImpl_BigInt();
}

class TightCoder extends ReaderWriterImpl {
	boolean bitpack;
	TightCoder(boolean bitpack) {
		this.bitpack= bitpack;
	}

	public void inspectElements(UserpReader reader) throws IOException {
		reader.pushTupleIterationState();
		reader.tupleType= (TupleType) reader.elemType;
		reader.elemCount= readElemCount(reader.tupleType, reader);
		reader.elemIdx= 0;
		reader.elemType= reader.tupleType.getElemType(0);
		if (bitpack)
			reader.enableBitpack(true);
	}

	public void nextElement(UserpReader reader) throws IOException {
		if (reader.elemType != null)
			reader.skipValue(); // throw away the value
		reader.elemType= (++reader.elemIdx >= reader.elemCount)? null : reader.tupleType.getElemType(reader.elemIdx);
	}

	public void endElements(UserpReader reader) throws IOException {
		while (reader.elemIdx < reader.elemCount)
			reader.nextElem();
		assert(reader.elemType == null);
		reader.popTupleIterationState();
	}

	public int loadValue(UserpReader reader) throws IOException {
		reader.inspectElems();
		int count= reader.getElemCount();
		Class stoClass= reader.tupleType.getNativeStoragePref();
		Object result;
		if (stoClass != null && stoClass != Object[].class) {
			result= Array.newInstance(stoClass.getComponentType(), count);
			for (int i=0; i<count; i++, reader.nextElem())
				Array.set(result, i, reader.readValue());
		}
		else {
			Object[] tmp= new Object[count];
			for (int i=0; i<count; i++, reader.nextElem())
				tmp[i]= (TypedData) reader.readValue();
			result= tmp;
		}
		reader.endElems();
		reader.valueObj= result;
		return reader.NATIVETYPE_OBJECT;
	}

	public void skipValue(UserpReader reader) throws IOException {
		reader.inspectElems();
		while (reader.elemType != null) {
			reader.skipValue();
			reader.nextElem();
		}
		reader.endElems();
	}

	static int readElemCount(TupleType t, UserpReader reader) throws IOException {
		int count= t.getElemCount();
		if (count < 0) {
			if (!reader.scalarIsLoaded)
				reader.readVarQty();
			else reader.scalarIsLoaded= false;
			count= reader.scalarAsInt();
		}
		return count;
	}

	static final TightCoder
		INSTANCE= new TightCoder(false),
		INSTANCE_BITPACK= new TightCoder(true);
}

class IndexedCoder extends ReaderWriterImpl {
	public void inspectElements(UserpReader reader) throws IOException {
		TightCoder.INSTANCE.inspectElements(reader);
		assert(!reader.scalarIsLoaded);
		reader.elemEndAddrs= new long[reader.elemCount];
		reader.readVarQty();
		int indexLen= reader.scalarAsInt();
		long indexEndAddr= reader.src.getSequencePos() + indexLen;
		long addr= indexEndAddr;
		for (int i=0; i<reader.elemCount; i++) {
			reader.readVarQty();
			addr+= reader.scalarAsInt();
			reader.elemEndAddrs[i]= addr;
		}
		skipToEndOf(indexEndAddr, reader);
		reader.elemIdx= -1;
		nextElement(reader);
	}

	public void nextElement(UserpReader reader) throws IOException {
		// skip to the end of the space reserved for the current element
		if (reader.elemIdx >= 0) // but not if this was called to prepare the first element
			skipToEndOf(reader.elemEndAddrs[reader.elemIdx], reader);

		if (++reader.elemIdx >= reader.elemCount)
			reader.elemType= null;
		else {
			reader.readVarQty();
			int metaLen= reader.scalarAsInt();
			reader.elemMeta= reader.makeCheckpoint();
			reader.src.forceSkip(metaLen);
			reader.elemType= reader.tupleType.getElemType(reader.elemIdx);
		}
	}

	public void endElements(UserpReader reader) throws IOException {
		skipToEndOf(reader.elemEndAddrs[reader.elemCount-1], reader);
		reader.elemType= null;
		reader.popTupleIterationState();
	}

	private void skipToEndOf(long addr, UserpReader reader) throws IOException {
		long padding= addr - reader.src.getSequencePos();
		if (padding < 0) {
			// Get some diagnostics.  Its inefficient, but this is an error condition.
			int elemIdx= -1;
			for (int i=0; i<reader.elemEndAddrs.length; i++)
				if (reader.elemEndAddrs[i] == addr) {
					elemIdx= i;
					break;
				}
			String elemName= elemIdx != -1? "Element "+elemIdx : "The index table";
			throw new UserpProtocolException("An indexed tuple (array or record) in the stream was improperly indexed.  "+elemName+" exceeded its space by "+(-padding)+" bytes.");
		}
		else if (padding > 0)
			reader.src.forceSkip(padding);
	}

	public int loadValue(UserpReader reader) throws IOException {
		return TightCoder.INSTANCE.loadValue(reader);
	}

	public void skipValue(UserpReader reader) throws IOException {
		int count= TightCoder.readElemCount((TupleType) reader.elemType, reader);
		reader.readVarQty();
		int indexLen= reader.scalarAsInt();
		long addr= reader.src.getSequencePos() + indexLen;
		for (int i=0; i<count; i++) {
			reader.readVarQty();
			addr+= reader.scalarAsInt();
		}
		reader.src.seek(addr);
	}

	static final IndexedCoder INSTANCE= new IndexedCoder();
}

class IndefiniteCoder extends ReaderWriterImpl {
	public void inspectElements(UserpReader reader) throws IOException {
		assert(!reader.scalarIsLoaded);
		reader.pushTupleIterationState();
		reader.tupleType= (TupleType) reader.elemType;
		reader.elemCount= reader.tupleType.getElemCount();
		reader.elemIdx= -1;
		nextElement(reader);
	}

	public void nextElement(UserpReader reader) throws IOException {
		reader.elemIdx++;
		reader.readQty(1);
		int metaLen= reader.scalarAsInt() - 1;
		if (metaLen >= 0) {
			if (metaLen > 0) {
				reader.elemMeta= reader.makeCheckpoint();
				reader.src.forceSkip(metaLen);
			}
			else
				reader.elemMeta= null;
			reader.elemType= reader.tupleType.getElemType(reader.elemIdx);
		}
		else {
			reader.elemCount= reader.elemIdx;
			reader.elemType= null;
		}
	}

	public void endElements(UserpReader reader) throws IOException {
		while (reader.elemIdx != reader.elemCount)
			reader.nextElem();
		reader.popTupleIterationState();
	}

	public int loadValue(UserpReader reader) throws IOException {
		LinkedList elems= new LinkedList();
		reader.inspectElems();
		Class stoClass= reader.tupleType.getNativeStoragePref();
		while (reader.getType() != null) {
			elems.add(reader.readValue());
			reader.nextElem();
		}
		reader.endElems();
		if (stoClass != Object[].class) {
			int count= elems.size();
			reader.valueObj= Array.newInstance(stoClass.getComponentType(), count);
			Iterator itr= elems.iterator();
			for (int i= 0; i<count; i++)
				Array.set(reader.valueObj, i, itr.next());
		}
		else
			reader.valueObj= elems.toArray();
		return reader.NATIVETYPE_OBJECT;
	}

	public void skipValue(UserpReader reader) throws IOException {
		reader.inspectElems();
		while (reader.elemType != null) {
			reader.skipValue();
			reader.nextElem();
		}
		reader.endElems();
	}

	static final IndefiniteCoder INSTANCE= new IndefiniteCoder();
}

//=============================================================================
// Optimized Variations

class NullImpl extends EnumImpl {
	public int loadValue(UserpReader reader) throws IOException {
		reader.scalarIsLoaded= false;
		reader.valueObj= null;
		return reader.NATIVETYPE_OBJECT;
	}

	static final NullImpl INSTANCE= new NullImpl();
}

class BoolImpl extends EnumImpl {
	public int loadValue(UserpReader reader) throws IOException {
		if (reader.scalarIsLoaded)
			reader.scalarIsLoaded= false;
		else
			reader.readQty(1);
		reader.value64= reader.scalar64;
		return reader.NATIVETYPE_BOOL;
	}

	static final BoolImpl INSTANCE= new BoolImpl();
}

class BigIntImpl extends UnionImpl_BigInt {
	public int loadValue(UserpReader reader) throws IOException {
		if (reader.scalarIsLoaded)
			reader.scalarIsLoaded= false;
		else
			reader.readVarQty();
		boolean readNegVal= (reader.scalar != null)? reader.scalar.signum() == 0 : reader.scalar64 == 0;
		if (readNegVal)
			NegWholeImpl.INSTANCE.loadValue(reader);
		else
			reader.valueObj= reader.scalarAsBigInt();
		return UserpReader.NATIVETYPE_OBJECT;
	}

	static final BigIntImpl INSTANCE= new BigIntImpl();
}

class SignedImpl extends UnionImpl_Long {
	int code, bits;
	SignedImpl(int code, int bits) { this.code= code; this.bits= bits; }

	public int loadValue(UserpReader reader) throws IOException {
		simple_NBits_impl(reader, bits);
		return code;
	}

	static final void simple_NBits_impl(UserpReader reader, int bits) throws IOException {
		if (reader.scalarIsLoaded) {
			reader.scalarIsLoaded= false;
			reader.value64= reader.scalarAsLong(bits);
		}
		else
			reader.value64= reader.rawRead(bits);
	}

	static final SignedImpl
		INSTANCE_BYTE= new SignedImpl(UserpReader.NATIVETYPE_BYTE, 8),
		INSTANCE_SHORT= new SignedImpl(UserpReader.NATIVETYPE_SHORT, 16),
		INSTANCE_INT= new SignedImpl(UserpReader.NATIVETYPE_INT, 32),
		INSTANCE_LONG= new SignedImpl(UserpReader.NATIVETYPE_LONG, 64);
}

class UnsignedImpl extends RangeImpl_Long {
	int code, bits;
	UnsignedImpl(int code, int bits) { this.code= code; this.bits= bits; }

	public int loadValue(UserpReader reader) throws IOException {
		SignedImpl.simple_NBits_impl(reader, bits);
		return code;
	}

	static final UnsignedImpl
		INSTANCE_UBYTE= new UnsignedImpl(UserpReader.NATIVETYPE_BYTE, 8),
		INSTANCE_USHORT= new UnsignedImpl(UserpReader.NATIVETYPE_SHORT, 16),
		INSTANCE_UINT= new UnsignedImpl(UserpReader.NATIVETYPE_INT, 32),
		INSTANCE_ULONG= new UnsignedImpl(UserpReader.NATIVETYPE_LONG, 64);
}

class FloatImpl extends TightCoder {
	FloatImpl() {
		super(true);
	}

	public int loadValue(UserpReader reader) throws IOException {
		if (reader.scalarIsLoaded) {
			reader.scalarIsLoaded= false;
			reader.value64= (reader.scalarAsInt(1)<<31) | reader.rawRead(31);
		}
		else
			reader.value64= reader.rawRead(32);
		return UserpReader.NATIVETYPE_FLOAT;
	}

	static final FloatImpl
		INSTANCE_FLOAT= new FloatImpl(),
		INSTANCE_DOUBLE= new DoubleImpl();
}

class DoubleImpl extends FloatImpl {
	public int loadValue(UserpReader reader) throws IOException {
		if (reader.scalarIsLoaded) {
			reader.scalarIsLoaded= false;
			reader.value64= (reader.scalarAsInt(1)<<63) | reader.rawRead(63);
		}
		else
			reader.value64= reader.rawRead(64);
		return UserpReader.NATIVETYPE_DOUBLE;
	}
}

class ByteArrayImpl extends TightCoder {
	int elemSize;
	ByteArrayImpl(int elemSize) {
		super(false);
		this.elemSize= elemSize;
	}

	public int loadValue(UserpReader reader) throws IOException {
		if (reader.scalarIsLoaded)
			reader.scalarIsLoaded= false;
		else
			reader.readVarQty();
		int len= reader.scalarAsInt();
		byte[] data= new byte[len*elemSize];
		reader.readBits(data, 0, data.length<<3);
		reader.valueObj= data;
		return UserpReader.NATIVETYPE_OBJECT;
	}

	static final ByteArrayImpl INSTANCE= new ByteArrayImpl(1);
}

class StringImpl extends ByteArrayImpl {
	String charSet;
	StringImpl(String charSet, int elemSize) {
		super(elemSize);
		this.charSet= charSet;
	}

	public int loadValue(UserpReader reader) throws IOException {
		super.loadValue(reader);
		reader.valueObj= new String(((byte[])reader.valueObj), charSet);
		return UserpReader.NATIVETYPE_OBJECT;
	}

	static final StringImpl
		INSTANCE_UTF8= new StringImpl("UTF-8", 1),
		INSTANCE_UTF16= new StringImpl("UTF-16BE", 2);
}

class DateImpl extends BigIntImpl {
	public int loadValue(UserpReader reader) throws IOException {
		super.loadValue(reader);
		reader.valueObj= new Date(((BigInteger)reader.valueObj).longValue());
		return UserpReader.NATIVETYPE_OBJECT;
	}

	static final DateImpl INSTANCE= new DateImpl();
}

class RelativeDateImpl extends DateImpl {
	public int loadValue(UserpReader reader) throws IOException {
		super.loadValue(reader);
		reader.valueObj= localToUTC(((BigInteger)reader.valueObj).longValue());
		return UserpReader.NATIVETYPE_OBJECT;
	}

	Date localToUTC(long val) {
		// we have a local time, and want to calculate a UTC time, but we need
		// to pass the UTC time as the parameter to the timezone, so the code
		// isn't really correct here.
		// There is a possibility that the local time doesn't exist in our time
		// zone, or that it exists twice.  I decided to just ignore the problem.
		java.util.TimeZone tz= java.util.TimeZone.getDefault();
		int ofs= tz.getOffset(val-tz.getOffset(val));
		return new Date(val-ofs);
	}

	long UTCToLocal(Date d) {
		// Convert the UTC time back to a local time.
		long timestamp= d.getTime();
		int ofs= java.util.TimeZone.getDefault().getOffset(timestamp);
		return timestamp+ofs;
	}
	static final RelativeDateImpl INSTANCE= new RelativeDateImpl();
}
