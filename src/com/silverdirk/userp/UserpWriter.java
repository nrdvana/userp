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
	CustomCodec nodeCodecOverride;
	CodecBuilder codecSet= new CodecBuilder(true);
	TupleStackEntry tupleState;
	Action eventActions[];
	LinkedList<String> warnings= new LinkedList<String>();

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
		FOREIGN_TYPE_USED,
		TYPE_DEFINED_MID_STREAM
	}

	public UserpWriter(UserpEncoder dest, Codec rootType) {
		this(dest, rootType, new CodecDescriptor[0]);
	}
	public UserpWriter(UserpEncoder dest, Codec rootType, CodecDescriptor[] knownTypes) {
		this.dest= dest;
		nodeCodec= rootType;
		for (int i=0; i<knownTypes.length; i++)
			codecSet.addDescriptor(knownTypes[i]);
		tupleState= new TupleStackEntry();
		tupleState.tupleCodec= RootSentinelCodec.INSTANCE;
		tupleState.elemIdx= -1;
		eventActions= new Action[Event.values().length];
		Arrays.fill(eventActions, Action.IGNORE);
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

	public CodecDescriptor getCodecFor(UserpType t) {
		return codecSet.getDescriptorFor(t);
	}

	public UserpWriter select(UserpType memberType) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleCodec.nextElement(this);
		nodeCodec.selectType(this, memberType);
		return this;
	}

	public UserpWriter select(CodecDescriptor unionMemberDesc) throws IOException {
		if (nodeCodec == null)
			tupleState.tupleCodec.nextElement(this);
		nodeCodec.selectType(this, unionMemberDesc);
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

	void checkWhetherCodecDefined(CodecDescriptor cd) {
		if (cd.encoderTypeCode < 0) {
			switch (eventActions[Event.TYPE_DEFINED_MID_STREAM.ordinal()]) {
			case ERROR: throw new RuntimeException("Type "+cd.type+" was not declared at the start of the stream, and mid-stream declarations are disabled.");
			case WARN:  addWarning("The type "+cd.type+" was declared mid-stream");
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
			tupleState.tupleCodec.nextElement(this);
		tupleStateStack.push(tupleState);
		tupleState= new TupleStackEntry();
		nodeCodec.beginTuple(this, elemCount);
		return this;
	}

	public UserpWriter endTuple() throws IOException {
		tupleState.tupleCodec.endTuple(this);
		tupleState= tupleStateStack.pop();
		nodeCodec= null;
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
			tupleState.tupleCodec.nextElement(this);
		if (nodeCodecOverride != null) {
			CustomCodec cc= nodeCodecOverride;
			nodeCodecOverride= null;
			cc.encode(this, sto, val_i64, val_obj);
		}
		else if (sto == ValRegister.StoType.LONG)
			nodeCodec.writeValue(this, val_i64);
		else
			nodeCodec.writeValue(this, val_obj);
		nodeCodec= null;
		return this;
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

	static class UserpWriterSpecialAccess {
		UserpWriter owner;
		UserpWriterSpecialAccess(UserpWriter owner) {
			this.owner= owner;
		}
		public UserpWriter beginTuple(int elemCount) throws IOException {
			owner.nodeCodec.beginTuple(owner, elemCount);
			return owner;
		}
		public UserpWriter write(Object value) throws IOException {
			owner.nodeCodec.writeValue(owner, value);
			return owner;
		}
		public UserpWriter write(long value) throws IOException {
			owner.nodeCodec.writeValue(owner, value);
			return owner;
		}
		public UserpWriter select(UserpType memberType) throws IOException {
			owner.nodeCodec.selectType(owner, memberType);
			return owner;
		}
		public UserpWriter select(CodecDescriptor unionMemberDesc) throws IOException {
			owner.nodeCodec.selectType(owner, unionMemberDesc);
			return owner;
		}
	}
}

class SentinelCodec extends TupleCodec {
	String errMsg_write, errMsg_read;

	protected SentinelCodec() {
		super(null, null, null);
	}

	Codec elemCodec(int idx) {
		throw new UnsupportedOperationException();
	}
	void writeValue(UserpWriter writer, Object val) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void writeValue(UserpWriter writer, long val) throws IOException {
		throw new IllegalStateException(errMsg_write);
	}
	void selectType(UserpWriter writer, CodecDescriptor whichUnionMember) throws IOException {
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

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		throw new IllegalStateException(errMsg_read);
	}
	void skipValue(UserpReader reader) throws IOException {
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

class RootSentinelCodec extends SentinelCodec {
	RootSentinelCodec() {
		errMsg_write= "Root element has been written; no further writing is allowed.";
		errMsg_read= "Root element has been read; no further reading is allowed.";
	}

	void nextElement(UserpWriter writer) throws IOException {
		writer.nodeCodec= this;
	}

	void nextElement(UserpReader reader) throws IOException {
		reader.nodeCodec= this;
	}

	static final RootSentinelCodec INSTANCE= new RootSentinelCodec();
}

class TupleEndSentinelCodec extends SentinelCodec {
	private TupleEndSentinelCodec() {
		errMsg_write= "Attempt to write more elements than allowed in current tuple (endTuple() expected)";
		errMsg_read= "Attempt to read more elements than exist in current tuple.  (closeTuple() expected)";
	}

	static final TupleEndSentinelCodec INSTANCE= new TupleEndSentinelCodec();
}
