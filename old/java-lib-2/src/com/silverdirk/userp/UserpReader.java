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
	CustomCodec nodeCodecOverride;
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

	final ValRegister valRegister= new ValRegister();

	public UserpReader(UserpDecoder src, Codec rootCodec) {
		this.src= src;
		nodeCodec= rootCodec;
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

	public final Object readValue() throws IOException {
		nodeCodec.readValue(this, valRegister);
		tupleState.tupleCodec.nextElement(this);
		switch (valRegister.sto) {
		case BOOL:   return Boolean.valueOf(valRegister.i64 != 0);
		case BYTE:   return new Byte((byte)valRegister.i64);
		case SHORT:  return new Short((short)valRegister.i64);
		case INT:    return new Integer((int)valRegister.i64);
		case LONG:   return new Long(valRegister.i64);
		case FLOAT:  return new Float(Float.intBitsToFloat((int)valRegister.i64));
		case DOUBLE: return new Double(Double.longBitsToDouble(valRegister.i64));
		case CHAR:   return new Character((char)valRegister.i64);
		case OBJECT: return valRegister.obj;
		default:
			throw new Error();
		}
	}

	public final void skipValue() throws IOException {
		nodeCodec.skipValue(this);
		tupleState.tupleCodec.nextElement(this);
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
		nodeCodec.readValue(this, valRegister);
		tupleState.tupleCodec.nextElement(this);
		switch (valRegister.sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return valRegister.i64;
		case FLOAT:  return (long) Float.intBitsToFloat((int)valRegister.i64);
		case DOUBLE: return (long) Double.longBitsToDouble(valRegister.i64);
		case OBJECT: return ((Number)valRegister.obj).longValue();
		default:
			throw new Error();
		}
	}

	public final float readAsFloat() throws IOException {
		nodeCodec.readValue(this, valRegister);
		tupleState.tupleCodec.nextElement(this);
		switch (valRegister.sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return (float) valRegister.i64;
		case FLOAT:  return Float.intBitsToFloat((int)valRegister.i64);
		case DOUBLE: return (float) Double.longBitsToDouble(valRegister.i64);
		case OBJECT: return ((Number)valRegister.obj).floatValue();
		default:
			throw new Error();
		}
	}

	public final double readAsDouble() throws IOException {
		nodeCodec.readValue(this, valRegister);
		tupleState.tupleCodec.nextElement(this);
		switch (valRegister.sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return (double) valRegister.i64;
		case FLOAT:  return (double) Float.intBitsToFloat((int)valRegister.i64);
		case DOUBLE: return Double.longBitsToDouble(valRegister.i64);
		case OBJECT: return ((Number)valRegister.obj).doubleValue();
		default:
			throw new Error();
		}
	}

	public final char readAsChar() throws IOException {
		return (char) readAsLong();
	}
}

