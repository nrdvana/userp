package com.silverdirk.userp;

import java.util.*;
import java.io.IOException;
import java.math.BigInteger;

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
		TupleImpl TupleImpl;
//		Checkpoint meta;
		boolean bitpack;
		int elemCount;
		int elemIdx;
		long[] elemEndAddrs;
	}

	public final ValRegister valRegister= new ValRegister();

	public UserpReader(UserpDecoder src, Codec rootCodec) {
		this.src= src;
		nodeCodec= rootCodec;
		tupleState= new TupleStackEntry();
		tupleState.TupleImpl= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= 0;
	}

	public final UserpType getType() {
		return nodeCodec.type;
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
		return nodeCodec.impl.inspectUnion(this);
	}

	public final int inspectTuple() throws IOException {
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		return nodeCodec.impl.inspectTuple(this);
	}

	public final void closeTuple() throws IOException {
		tupleState.TupleImpl.closeTuple(this);
		tupleState= tupleStateStack.pop();
		tupleState.TupleImpl.nextElement(this);
	}

	public final Object readValue() throws IOException {
		nodeCodec.impl.readValue(this, valRegister);
		tupleState.TupleImpl.nextElement(this);
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
		nodeCodec.impl.skipValue(this);
		tupleState.TupleImpl.nextElement(this);
	}

	public final boolean readAsBool() throws IOException {
		loadValue();
		return valRegister.asBool();
	}

	public final byte readAsByte() throws IOException {
		loadValue();
		return valRegister.asByte();
	}

	public final short readAsShort() throws IOException {
		loadValue();
		return valRegister.asShort();
	}

	public final int readAsInt() throws IOException {
		loadValue();
		return valRegister.asInt();
	}

	public final long readAsLong() throws IOException {
		loadValue();
		return valRegister.asLong();
	}

	public final float readAsFloat() throws IOException {
		loadValue();
		return valRegister.asFloat();
	}

	public final double readAsDouble() throws IOException {
		loadValue();
		return valRegister.asDouble();
	}

	public final char readAsChar() throws IOException {
		loadValue();
		return valRegister.asChar();
	}

	public final BigInteger readAsBigInt() throws IOException {
		loadValue();
		return valRegister.asBigInt();
	}
}

