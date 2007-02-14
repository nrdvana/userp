package com.silverdirk.userp;

import java.io.IOException;
import java.math.BigInteger;
import java.util.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Userp Reader</p>
 * <p>Description: Class that allows high-level reading of the Userp protocol.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class UserpReader extends UserpDataReader {
	HashMap customCodecs;

	UserpType elemType;
	TupleType tupleType;
	Checkpoint elemMeta;
	int elemCount;
	int elemIdx;
	long[] elemEndAddrs;

	LinkedList tupleStateStack= new LinkedList();
	static class TupleStackEntry {
		TupleType tupleType;
		Checkpoint meta;
		boolean bitpack;
		int elemCount;
		int elemIdx;
		long[] elemEndAddrs;
	}

	Object valueObj;
	long value64;

	static final int
		NATIVETYPE_BOOL= 0,
		NATIVETYPE_BYTE= 1,
		NATIVETYPE_SHORT= 2,
		NATIVETYPE_INT= 3,
		NATIVETYPE_LONG= 4,
		NATIVETYPE_FLOAT= 5,
		NATIVETYPE_DOUBLE= 6,
		NATIVETYPE_CHAR= 7,
		NATIVETYPE_OBJECT= 8;
	static final int[] nativeTypeByteLenMap= new int[] {
		UserpReader.NATIVETYPE_BYTE, UserpReader.NATIVETYPE_SHORT,
		UserpReader.NATIVETYPE_INT,  UserpReader.NATIVETYPE_INT,
		UserpReader.NATIVETYPE_LONG, UserpReader.NATIVETYPE_LONG,
		UserpReader.NATIVETYPE_LONG, UserpReader.NATIVETYPE_LONG,
	};
	int getTypeForBitLen(int bits) {
		return nativeTypeByteLenMap[bits>>3];
	}

	public UserpReader(ByteSequence seq, ByteSequence.IStream src, TypeMap typeMap) {
		super(seq, src, typeMap);
		customCodecs= new HashMap();
	}

	public final UserpType getType() {
		return elemType;
	}

	public final void getMetaReader() {
		throw new UnsupportedOperationException();
	}

	public int getTreeDepth() {
		return tupleStateStack.size();
	}

	public String getElemPath() {
		StringBuffer sb= new StringBuffer();
		Iterator itr= tupleStateStack.iterator();
		while (itr.hasNext()) {
			TupleStackEntry tse= (TupleStackEntry) itr.next();
			if (tse.tupleType.isArray())
				sb.append('[').append(tse.elemIdx).append(']');
			else
				sb.append('.').append(((RecordType)tse.tupleType).getFieldName(tse.elemIdx));
		}
		return sb.toString();
	}

	public final boolean hasDerivative() {
		return elemType.isRange() || elemType.isUnion() || elemType == FundamentalTypes.TAny;
	}

	public final void inspectDerivative() throws IOException {
		elemType.impl.inspectActualType(this);
	}

	public final boolean hasElems() {
		return elemType.isTuple();
	}

	public final int getElemCount() {
		return elemCount;
	}

	public final void inspectElems() throws IOException {
		elemType.impl.inspectElements(this);
	}

	public final boolean nextElem() throws IOException {
		tupleType.impl.nextElement(this);
		return elemType != null;
	}

	public final void endElems() throws IOException {
		tupleType.impl.endElements(this);
		elemType= null;
	}

	void startFreshElement(UserpType elemType) throws IOException {
		this.elemType= elemType;
	}

	void pushTupleIterationState() {
		TupleStackEntry se= new TupleStackEntry();
		se.tupleType= tupleType;
		se.meta= elemMeta;
		se.bitpack= bitpack;
		se.elemCount= elemCount;
		se.elemIdx= elemIdx;
		se.elemEndAddrs= elemEndAddrs;
		tupleStateStack.addLast(se);
	}

	void popTupleIterationState() {
		TupleStackEntry se= (TupleStackEntry) tupleStateStack.removeLast();
		tupleType= se.tupleType;
		elemType= null;
		elemMeta= se.meta;
		if (bitpack != se.bitpack)
			enableBitpack(se.bitpack);
		elemCount= se.elemCount;
		elemIdx= se.elemIdx;
		elemEndAddrs= se.elemEndAddrs;
	}

	private final int performLoadValue() throws IOException {
		if (elemType == null)
			throw new RuntimeException("Value for this element was already retrieved");
		int valType;
		CustomCodec codec= (CustomCodec) customCodecs.get(elemType.handle);
		if (codec != null) {
			valueObj= codec.decode(this);
			valType= NATIVETYPE_OBJECT;
		}
		else
			valType= elemType.impl.loadValue(this);
		elemType= null;
		return valType;
	}

	public final Object readValue() throws IOException {
		switch (performLoadValue()) {
		case NATIVETYPE_BOOL:   return Boolean.valueOf(value64 != 0);
		case NATIVETYPE_BYTE:   return new Byte((byte)value64);
		case NATIVETYPE_SHORT:  return new Short((short)value64);
		case NATIVETYPE_INT:    return new Integer((int)value64);
		case NATIVETYPE_LONG:   return new Long(value64);
		case NATIVETYPE_FLOAT:  return new Float(Float.intBitsToFloat((int)value64));
		case NATIVETYPE_DOUBLE: return new Double(Double.longBitsToDouble(value64));
		case NATIVETYPE_CHAR:   return new Character((char)value64);
		case NATIVETYPE_OBJECT: return valueObj;
		default:
			throw new RuntimeException();
		}
	}

	public final void skipValue() throws IOException {
		if (elemType == null)
			throw new RuntimeException("Value for this element was already retrieved");
		elemType.impl.skipValue(this);
		elemType= null;
	}

	public final boolean readValAsBool() throws IOException {
		return ((performLoadValue() == NATIVETYPE_BOOL)? value64 : ((Number)readValue()).intValue()) == 0;
	}

	public final byte readValAsByte() throws IOException {
		return (performLoadValue() == NATIVETYPE_BYTE)? (byte) value64 : ((Number)readValue()).byteValue();
	}

	public final short readValAsShort() throws IOException {
		return (performLoadValue() == NATIVETYPE_SHORT)? (short) value64 : ((Number)readValue()).shortValue();
	}

	public final int readValAsInt() throws IOException {
		return (performLoadValue() == NATIVETYPE_INT)? (int) value64 : ((Number)readValue()).intValue();
	}

	public final long readValAsLong() throws IOException {
		return (performLoadValue() == NATIVETYPE_LONG)? value64 : ((Number)readValue()).longValue();
	}

	public final float readValAsFloat() throws IOException {
		return (performLoadValue() == NATIVETYPE_FLOAT)? Float.intBitsToFloat((int) value64)
			: ((Number)readValue()).floatValue();
	}

	public final double readValAsDouble() throws IOException {
		return (performLoadValue() == NATIVETYPE_DOUBLE)? Double.longBitsToDouble(value64)
			: ((Number)readValue()).doubleValue();
	}

	public final char readValAsChar() throws IOException {
		return (performLoadValue() == NATIVETYPE_CHAR)? (char) value64 : (char)((Number)readValue()).intValue();
	}
}

interface ReaderImpl {
	public void inspectActualType(UserpReader reader) throws IOException;
	public void inspectElements(UserpReader reader) throws IOException;
	public void nextElement(UserpReader reader) throws IOException;
	public void endElements(UserpReader reader) throws IOException;
	public int loadValue(UserpReader reader) throws IOException;
	public void skipValue(UserpReader reader) throws IOException;
}

