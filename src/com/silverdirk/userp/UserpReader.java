package com.silverdirk.userp;

import java.util.*;
import java.io.IOException;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class UserpReader {
	UserpDecoder src;
	Codec nodeCodec;
	TupleStackEntry tupleState;

	Stack<TupleStackEntry> tupleStateStack= new Stack<TupleStackEntry>();
	static class TupleStackEntry {
		TupleCodec tupleCodec;
//		Checkpoint meta;
		boolean bitpack;
		int elemCount;
		int elemIdx;
		long[] elemEndAddrs;
	}

	Object valRegister_o;
	long valRegister_64;

	public UserpReader(UserpDecoder src, Codec rootCodec) {
		this.src= src;
		this.nodeCodec= rootCodec;
		tupleState= new TupleStackEntry();
		tupleState.tupleCodec= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= 0;
	}

	public final UserpType getType() {
		return nodeCodec.getType();
	}

	public final void getMetaReader() {
		throw new Error("Unimplemented");
	}

	public final boolean isScalar() {
		return nodeCodec instanceof ScalarCodec;
	}

	public final boolean isUnion() {
		return nodeCodec instanceof UnionCodec;
	}

	public final boolean isTuple() {
		return nodeCodec instanceof TupleCodec;
	}

	public final int inspectUnion() throws IOException {
		return nodeCodec.inspectUnion(this);
	}

	public final int inspectTuple() throws IOException {
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		return nodeCodec.inspectTuple(this);
	}

	public final void closeTuple() throws IOException {
		tupleState.tupleCodec.closeTuple(this);
		tupleState= tupleStateStack.pop();
		tupleState.tupleCodec.nextElement(this);
	}

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

	public final Object readValue() throws IOException {
		int stoClass= nodeCodec.readValue(this);
		tupleState.tupleCodec.nextElement(this);
		switch (stoClass) {
		case NATIVETYPE_BOOL:   return Boolean.valueOf(valRegister_64 != 0);
		case NATIVETYPE_BYTE:   return new Byte((byte)valRegister_64);
		case NATIVETYPE_SHORT:  return new Short((short)valRegister_64);
		case NATIVETYPE_INT:    return new Integer((int)valRegister_64);
		case NATIVETYPE_LONG:   return new Long(valRegister_64);
		case NATIVETYPE_FLOAT:  return new Float(Float.intBitsToFloat((int)valRegister_64));
		case NATIVETYPE_DOUBLE: return new Double(Double.longBitsToDouble(valRegister_64));
		case NATIVETYPE_CHAR:   return new Character((char)valRegister_64);
		case NATIVETYPE_OBJECT: return valRegister_o;
		default:
			throw new Error();
		}
	}

	public final void skipValue() throws IOException {
		tupleState.tupleCodec.nextElement(this);
		nodeCodec.skipValue(this);
	}

	public final boolean readAsBool() throws IOException {
		return readAsLong() != 0;
	}

	public final byte readAsByte() throws IOException {
		return (byte) readAsLong();
	}

	public final short readAsShort() throws IOException {
		return (short) readAsLong();
	}

	public final int readAsInt() throws IOException {
		return (int) readAsLong();
	}

	public final long readAsLong() throws IOException {
		int stoClass= nodeCodec.readValue(this);
		tupleState.tupleCodec.nextElement(this);
		switch (stoClass) {
		case NATIVETYPE_BOOL:
		case NATIVETYPE_BYTE:
		case NATIVETYPE_SHORT:
		case NATIVETYPE_INT:
		case NATIVETYPE_CHAR:
		case NATIVETYPE_LONG:   return valRegister_64;
		case NATIVETYPE_FLOAT:  return (long) Float.intBitsToFloat((int)valRegister_64);
		case NATIVETYPE_DOUBLE: return (long) Double.longBitsToDouble(valRegister_64);
		case NATIVETYPE_OBJECT: return ((Number)valRegister_o).longValue();
		default:
			throw new Error();
		}
	}

	public final float readAsFloat() throws IOException {
		int stoClass= nodeCodec.readValue(this);
		tupleState.tupleCodec.nextElement(this);
		switch (stoClass) {
		case NATIVETYPE_BOOL:
		case NATIVETYPE_BYTE:
		case NATIVETYPE_SHORT:
		case NATIVETYPE_INT:
		case NATIVETYPE_CHAR:
		case NATIVETYPE_LONG:   return (float) valRegister_64;
		case NATIVETYPE_FLOAT:  return Float.intBitsToFloat((int)valRegister_64);
		case NATIVETYPE_DOUBLE: return (float) Double.longBitsToDouble(valRegister_64);
		case NATIVETYPE_OBJECT: return ((Number)valRegister_o).floatValue();
		default:
			throw new Error();
		}
	}

	public final double readAsDouble() throws IOException {
		int stoClass= nodeCodec.readValue(this);
		tupleState.tupleCodec.nextElement(this);
		switch (stoClass) {
		case NATIVETYPE_BOOL:
		case NATIVETYPE_BYTE:
		case NATIVETYPE_SHORT:
		case NATIVETYPE_INT:
		case NATIVETYPE_CHAR:
		case NATIVETYPE_LONG:   return (double) valRegister_64;
		case NATIVETYPE_FLOAT:  return (double) Float.intBitsToFloat((int)valRegister_64);
		case NATIVETYPE_DOUBLE: return Double.longBitsToDouble(valRegister_64);
		case NATIVETYPE_OBJECT: return ((Number)valRegister_o).doubleValue();
		default:
			throw new Error();
		}
	}

	public final char readAsChar() throws IOException {
		return (char) readAsLong();
	}

	static final int[] nativeTypeByteLenMap= new int[] {
		UserpReader.NATIVETYPE_BYTE, UserpReader.NATIVETYPE_SHORT,
		UserpReader.NATIVETYPE_INT,  UserpReader.NATIVETYPE_INT,
		UserpReader.NATIVETYPE_LONG, UserpReader.NATIVETYPE_LONG,
		UserpReader.NATIVETYPE_LONG, UserpReader.NATIVETYPE_LONG,
	};
	public static int getTypeForBitLen(int bits) {
		return nativeTypeByteLenMap[bits>>3];
	}
}
