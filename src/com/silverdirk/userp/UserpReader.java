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
	boolean codecOverrideInProgress= false;

	Stack<TupleStackEntry> tupleStateStack= new Stack<TupleStackEntry>();
	static class TupleStackEntry {
		TupleImpl tupleImpl;
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
		tupleState.tupleImpl= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= 0;
	}

	public void reInit(Codec newRootCodec) {
		if (tupleStateStack.size() != 0)
			throw new RuntimeException("Current tree is not finished.");
		nodeCodec= newRootCodec;
		tupleState.tupleImpl= RootSentinelCodec.INSTANCE;
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
		codecOverrideInProgress= false;
		return nodeCodec.impl.inspectUnion(this);
	}

	public final int inspectTuple() throws IOException {
		codecOverrideInProgress= false;
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		return nodeCodec.impl.inspectTuple(this);
	}

	public final void closeTuple() throws IOException {
		tupleState.tupleImpl.closeTuple(this);
		tupleState= tupleStateStack.pop();
		tupleState.tupleImpl.nextElement(this);
	}

	public final void loadValue() throws IOException {
		if (nodeCodec.implOverride != null && !codecOverrideInProgress) {
			codecOverrideInProgress= true;
			int startIdx= tupleState.elemIdx;
			int startDepth= tupleStateStack.size();
			CodecOverride co= nodeCodec.implOverride;
			co.loadValue(this);
			if (tupleStateStack.size() != startDepth)
				throw new CodecOverride.ImplementationError(co, startDepth, tupleStateStack.size(), true);
			if (tupleState.elemIdx-startIdx != 1)
				throw new CodecOverride.ImplementationError(co, tupleState.elemIdx-startIdx, true);
			codecOverrideInProgress= false;
		}
		else {
			int startIdx= tupleState.elemIdx;
			nodeCodec.impl.readValue(this, valRegister);
			if (tupleState.elemIdx == startIdx)
				tupleState.tupleImpl.nextElement(this);
		}
	}

	public final void skipValue() throws IOException {
		nodeCodec.impl.skipValue(this);
		tupleState.tupleImpl.nextElement(this);
	}

	public final Object readValue() throws IOException {
		loadValue();
		return valRegister.asObject();
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

