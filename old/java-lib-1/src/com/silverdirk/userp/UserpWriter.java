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
public class UserpWriter {
	UserpEncoder dest;
	CodecBuilder codecSet= new CodecBuilder(true);
	Action eventActions[];
	LinkedList<String> warnings= new LinkedList<String>();
	TupleStackEntry tupleState;
	Codec nodeCodec;
	boolean codecOverrideInProgress= false;
	boolean flushAtEnd= true;

	Stack<TupleStackEntry> tupleStateStack= new Stack<TupleStackEntry>();
	static class TupleStackEntry {
		TupleImpl tupleImpl;
//		Checkpoint meta;
		boolean bitpack;
		int elemCount;
		int elemIdx;
		ByteReservoir[] indexElems;
	}

	static enum Action {
		IGNORE, WARN, ERROR
	}
	static enum Event {
		FOREIGN_TYPE_USED,
		TYPE_DEFINED_MID_STREAM
	}

	public UserpWriter(UserpEncoder dest, Codec rootType) {
		this(dest, rootType, new Codec[0]);
	}
	public UserpWriter(UserpEncoder dest, Codec rootType, Codec[] knownTypes) {
		this.dest= dest;
		nodeCodec= rootType;
		for (int i=0; i<knownTypes.length; i++)
			codecSet.addDescriptor(knownTypes[i]);
		tupleState= new TupleStackEntry();
		tupleState.tupleImpl= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= -1;
		eventActions= new Action[Event.values().length];
		Arrays.fill(eventActions, Action.IGNORE);
	}

	public void reInit(Codec rootType) {
		if (tupleStateStack.size() != 0)
			throw new RuntimeException("Current tree is not finished.");
		nodeCodec= rootType;
		tupleState.tupleImpl= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= -1;
	}

	public void setEventAction(Event e, Action a) {
		int idx= e.ordinal();
		if (eventActions[idx] != a)
			eventActions[idx]= a;
	}

	void addWarning(String msg) {
		warnings.add(msg);
	}

	public void addType(UserpType t) {
		codecSet.addType(t);
	}

	public void addType(UnionType t, boolean bitpack, boolean[] memberInlined) {
		codecSet.addType(t, bitpack, memberInlined);
	}

	public void addType(TupleType t, TupleCoding encodingMode) {
		codecSet.addType(t, encodingMode);
	}

	public Codec getCodecFor(UserpType t) {
		return codecSet.getCodecFor(t);
	}

	public UserpWriter select(UserpType memberType) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleImpl.nextElement(this);
		nodeCodec.impl.selectType(this, memberType);
		codecOverrideInProgress= false;
		return this;
	}

	public UserpWriter select(Codec unionMemberDesc) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleImpl.nextElement(this);
		nodeCodec.impl.selectType(this, unionMemberDesc);
		codecOverrideInProgress= false;
		return this;
	}

	void checkWhetherTypeKnown(UserpType t) {
		if (!codecSet.typeMap.containsKey(t.handle)) {
			switch (eventActions[Event.FOREIGN_TYPE_USED.ordinal()]) {
			case ERROR: throw new RuntimeException("Cannot encode "+t+" because this type has not explicitly been added to this stream");
			case WARN:  addWarning("The type "+t+" was implicitly added to the stream");
			case IGNORE:
			}
		}
	}

	void checkWhetherCodecDefined(Codec c) {
		if (c.encoderTypeCode < 0) {
			switch (eventActions[Event.TYPE_DEFINED_MID_STREAM.ordinal()]) {
			case ERROR: throw new RuntimeException("Type "+c.type+" was not declared at the start of the stream, and mid-stream declarations are disabled.");
			case WARN:  addWarning("The type "+c.type+" was declared mid-stream");
			case IGNORE:
			}
		}
	}

	public UserpWriter beginTuple() throws IOException {
		beginTuple(-1);
		return this;
	}

	public UserpWriter beginTuple(int elemCount) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleImpl.nextElement(this);
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		nodeCodec.impl.beginTuple(this, elemCount);
		codecOverrideInProgress= false;
		return this;
	}

	public UserpWriter endTuple() throws IOException {
		tupleState.tupleImpl.endTuple(this);
		tupleState= tupleStateStack.pop();
		nodeCodec= null;
		// check for end of writing
		if (tupleStateStack.size() == 0)
			endStream();
		return this;
	}

	public final UserpWriter write(Object value) throws IOException {
		return write(ValRegister.StoType.OBJECT, 0, value);
	}

	public final UserpWriter write(long value) throws IOException {
		return write(ValRegister.StoType.LONG, value, null);
	}

	private UserpWriter write(ValRegister.StoType sto, long val_i64, Object val_obj) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleImpl.nextElement(this);
		if (nodeCodec.implOverride != null && !codecOverrideInProgress) {
			codecOverrideInProgress= true;
			int startIdx= tupleState.elemIdx;
			int startDepth= tupleStateStack.size();
			CodecOverride co= nodeCodec.implOverride;
			co.writeValue(this, sto, val_i64, val_obj);
			if (tupleStateStack.size() != startDepth)
				throw new CodecOverride.ImplementationError(co, startDepth, tupleStateStack.size(), false);
			if (tupleState.elemIdx != startIdx || nodeCodec != null)
				throw new CodecOverride.ImplementationError(co, nodeCodec != null? 0 : tupleState.elemIdx-startIdx+1, false);
			codecOverrideInProgress= false;
		}
		else
			nodeCodec.impl.writeValue(this, sto, val_i64, val_obj);
		nodeCodec= null;
		// check for end of writing
		if (tupleStateStack.size() == 0)
			endStream();
		return this;
	}

	/** After completing the root element, write out any leftover bits,
	 * and flush the underlying stream.
	 */
	private void endStream() throws IOException {
		if (flushAtEnd) {
			dest.flushToByteBoundary();
			dest.flush();
		}
	}

	public final UserpWriter writeBool(boolean value) throws IOException {
		return select(Userp.TBool).write(ValRegister.StoType.BOOL, value?1:0, null);
	}

	public final UserpWriter writeByte(byte value) throws IOException {
		return select(Userp.TInt8).write(ValRegister.StoType.BYTE, value&0xFF, null);
	}

	public final UserpWriter writeShort(short value) throws IOException {
		return select(Userp.TInt16).write(ValRegister.StoType.SHORT, value&0xFFFF, null);
	}

	public final UserpWriter writeInt(int value) throws IOException {
		return select(Userp.TInt32).write(ValRegister.StoType.INT, value&0xFFFFFFFFL, null);
	}

	public final UserpWriter writeLong(long value) throws IOException {
		return select(Userp.TInt64).write(ValRegister.StoType.LONG, value, null);
	}

	public final UserpWriter writeFloat(float value) throws IOException {
		return select(Userp.TInt32).write(ValRegister.StoType.INT, Float.floatToIntBits(value), null);
	}

	public final UserpWriter writeDouble(double value) throws IOException {
		return select(Userp.TInt64).write(ValRegister.StoType.LONG, Double.doubleToLongBits(value), null);
	}

	public final UserpWriter writeByteArray(byte[] value) throws IOException {
		return select(Userp.TByteArray).write(ValRegister.StoType.OBJECT, 0, value);
	}

	public final UserpWriter writeChar(char value) throws IOException {
		return select(Userp.TInt16u).write(ValRegister.StoType.SHORT, value, null);
	}

	public final UserpWriter writeString(String value) throws IOException {
		return select(Userp.TStrUTF8).write(ValRegister.StoType.OBJECT, 0, value);
	}
}

class SentinelCodec extends Codec {
	SentinelCodec(SentinelCodecImpl impl) {
		this.impl= impl;
	}
	void resolveDescriptorRefs(CodecBuilder cb) {}
	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) {}
	protected void initScalarRange() {}
	protected void initCodec() {}

	protected void serializeFields(UserpWriter writer) throws IOException {
		throw new Error("SentinelCodec not serializable");
	}
}

class SentinelCodecImpl extends TupleImpl {
	String errMsg_write, errMsg_read;

	protected SentinelCodecImpl() {
		super(null);
		descriptor= new SentinelCodec(this);
	}

	Codec elemCodec(int idx) {
		throw new UnsupportedOperationException();
	}
	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void nextElement(UserpWriter writer) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void endTuple(UserpWriter writer) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	public void skipValue(UserpReader reader) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	int inspectUnion(UserpReader reader) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	int inspectTuple(UserpReader reader) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	void nextElement(UserpReader reader) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	void closeTuple(UserpReader reader) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
}

class RootSentinelCodec extends SentinelCodecImpl {
	RootSentinelCodec() {
		errMsg_write= "Root element has been written; no further writing is allowed.";
		errMsg_read= "Root element has been read; no further reading is allowed.";
	}

	void nextElement(UserpWriter writer) throws IOException {
		writer.nodeCodec= descriptor;
		writer.tupleState.elemIdx++; // satisfy some assertions
	}

	void nextElement(UserpReader reader) throws IOException {
		reader.nodeCodec= descriptor;
		reader.tupleState.elemIdx++; // satisfy some assertions
	}

	static final RootSentinelCodec INSTANCE= new RootSentinelCodec();
}

class TupleEndSentinelCodec extends SentinelCodecImpl {
	private TupleEndSentinelCodec() {
		errMsg_write= "Attempt to write more elements than allowed in current tuple (endTuple() expected)";
		errMsg_read= "Attempt to read more elements than exist in current tuple.  (closeTuple() expected)";
	}

	static final TupleEndSentinelCodec INSTANCE= new TupleEndSentinelCodec();
}
