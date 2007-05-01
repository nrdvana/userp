package com.silverdirk.userp;

import java.util.*;
import java.math.BigInteger;
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
	Codec nodeCodec;
	TupleStackEntry tupleState;
	Action eventActions[];

	Stack<TupleStackEntry> tupleStateStack= new Stack<TupleStackEntry>();
	static class TupleStackEntry {
		TupleCodec tupleCodec;
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
		UNDEFINED_TYPE_USED
	}

	public UserpWriter(UserpEncoder dest, Codec rootType) {
		this.dest= dest;
		nodeCodec= rootType;
		tupleState= new TupleStackEntry();
		tupleState.tupleCodec= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= -1;
		eventActions= new Action[Event.values().length];
		Arrays.fill(eventActions, Action.IGNORE);
	}

	public void setEventAction(Event e, Action a) {
		eventActions[e.ordinal()]= a;
	}

	void addWarning(String msg) {
		throw new Error("Unimplemented");
	}

	public Codec addType(UserpType t) {
		throw new Error("Unimplemented");
	}

	public Codec addType(UnionType t, boolean bitpack, boolean[] memberInlineFlags) {
		throw new Error("Unimplemented");
	}

	public Codec addType(TupleType t, TupleCoding defaultCoding) {
		throw new Error("Unimplemented");
	}

	public UserpWriter select(UserpType memberType) throws IOException {
		nodeCodec.selectType(this, memberType);
		return this;
	}

	public UserpWriter select(Codec unionMemberCodec) throws IOException {
		nodeCodec.selectType(this, unionMemberCodec);
		return this;
	}

	public UserpWriter beginTuple() throws IOException {
		beginTuple(-1);
		return this;
	}

	public UserpWriter beginTuple(int elemCount) throws IOException {
		tupleState.tupleCodec.nextElement(this);
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		nodeCodec.beginTuple(this, elemCount);
		return this;
	}

	public UserpWriter endTuple() throws IOException {
		tupleState.tupleCodec.endTuple(this);
		tupleState= tupleStateStack.pop();
		return this;
	}

	public UserpWriter write(Object value) throws IOException {
		tupleState.tupleCodec.nextElement(this);
		nodeCodec.writeValue(this, value);
		return this;
	}

	public UserpWriter write(long value) throws IOException {
		tupleState.tupleCodec.nextElement(this);
		nodeCodec.writeValue(this, value);
		return this;
	}

	public final UserpWriter writeBool(boolean value) throws IOException {
		select(Userp.TBool);
		return write(value?1:0);
	}

	public final UserpWriter writeByte(byte value) throws IOException {
		select(Userp.TInt8);
		return write(value&0xFF);
	}

	public final UserpWriter writeShort(short value) throws IOException {
		select(Userp.TInt16);
		return write(value&0xFFFF);
	}

	public final UserpWriter writeInt(int value) throws IOException {
		select(Userp.TInt32);
		return write(value&0xFFFFFFFFL);
	}

	public final UserpWriter writeLong(long value) throws IOException {
		select(Userp.TInt64);
		return write(value);
	}

	public final UserpWriter writeFloat(float value) throws IOException {
		select(Userp.TInt32);
		return write(Float.floatToIntBits(value));
	}

	public final UserpWriter writeDouble(double value) throws IOException {
		select(Userp.TInt64);
		return write(Double.doubleToLongBits(value));
	}

	public final UserpWriter writeByteArray(byte[] value) throws IOException {
		select(Userp.TByteArray);
		return write(value);
	}

	public final UserpWriter writeChar(char value) throws IOException {
		select(Userp.TInt16u);
		return write(value);
	}

	public final UserpWriter writeString(String value) throws IOException {
		select(Userp.TStrUTF8);
		return write(value);
	}
}

class RootSentinelCodec extends TupleCodec {
	private RootSentinelCodec() {
		super(BigInteger.ZERO, null, null);
	}
	void resolveRefs(UserpDecoder.TypeMap codeMap, Object params) {}
	void resolveRefs(CodecCollection codecs) {}
	protected void internal_serialize(UserpWriter dest) throws IOException {
		throw new UnsupportedOperationException();
	}
	Codec elemCodec(int idx) {
		throw new UnsupportedOperationException();
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}
	void nextElement(UserpWriter writer) throws IOException {
		if (++writer.tupleState.elemIdx > 0)
			writer.nodeCodec= this;
	}
	void endTuple(UserpWriter writer) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}

	int readValue(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}
	void skipValue(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}

	int inspectTuple(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}
	void nextElement(UserpReader reader) throws IOException {
		if (++reader.tupleState.elemIdx > 0)
			reader.nodeCodec= this;
	}
	void closeTuple(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}

	static final String doneMsg_write= "Root element has been written; no further writing is allowed.";
	static final String doneMsg_read= "Root element has been read; no further reading is allowed.";

	static final RootSentinelCodec INSTANCE= new RootSentinelCodec();
}

class TupleEndSentinelCodec extends Codec {
	private TupleEndSentinelCodec() { super(BigInteger.ZERO); }
	protected void internal_serialize(UserpWriter dest) throws IOException {}
	void resolveRefs(UserpDecoder.TypeMap codeMap, Object params) {}
	void resolveRefs(CodecCollection codecs) {}
	UserpType getType() { return null; }

	BigInteger getScalarRange() {
		return scalarRange;
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}
	void writeValue(UserpWriter writer, long val) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}
	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new IllegalStateException(doneMsg_write);
	}

	int readValue(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}
	void skipValue(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}
	int inspectUnion(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}
	int inspectTuple(UserpReader reader) throws IOException {
		throw new IllegalStateException(doneMsg_read);
	}

	static final String doneMsg_write= "Attempt to write more elements than allowed in current tuple (endTuple() expected)";
	static final String doneMsg_read= "Attempt to read more elements than exist in current tuple.  (closeTuple() expected)";

	static final TupleEndSentinelCodec INSTANCE= new TupleEndSentinelCodec();
}
