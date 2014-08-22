package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.*;
import java.io.*;
import com.silverdirk.userp.RecordType.Field;
import com.silverdirk.userp.EnumType.Range;
import com.silverdirk.userp.UserpWriter.Action;
import com.silverdirk.userp.UserpWriter.Event;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
abstract class Codec {
	CodecDescriptor descriptor;

	protected Codec(CodecDescriptor cd) {
		descriptor= cd;
	}

	final CodecDescriptor getDescriptor() {
		return descriptor;
	}

	abstract UserpType getType();

	abstract void writeValue(UserpWriter writer, Object val) throws IOException;
	void writeValue(UserpWriter writer, long val) throws IOException {
		throw new RuntimeException("Cannot convert java-type 'long' to a value of type "+getType());
	}
	void selectType(UserpWriter writer, UserpType memberType) throws IOException {
		writer.checkWhetherTypeKnown(memberType);
		selectType(writer, writer.codecSet.getDescriptorFor(memberType));
	}
	void selectType(UserpWriter writer, CodecDescriptor whichUnionMember) throws IOException {
		throw new RuntimeException("Operation SelectType not applicable to type "+getType());
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new RuntimeException("Operation BeginTuple not applicable to type "+getType());
	}

	abstract void readValue(UserpReader reader, ValRegister vr) throws IOException;
	abstract void skipValue(UserpReader reader) throws IOException;
	int inspectUnion(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectUnion not applicable to type "+getType());
	}
	int inspectTuple(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectTuple not applicable to type "+getType());
	}
}

abstract class ScalarCodec extends Codec {
	ScalarType type;

	protected ScalarCodec(ScalarType t, CodecDescriptor cd) {
		super(cd);
		this.type= t;
	}

	public UserpType getType() {
		return type;
	}
}

class IntegerCodec extends ScalarCodec {
	protected IntegerCodec(ScalarType t, CodecDescriptor cd) {
		super(t, cd);
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof BigInteger) {
			BigInteger val_bi= (BigInteger) val;
			if (val_bi.signum() >= 0)
				writer.dest.writeVarQty(val_bi.add(BigInteger.ONE));
			else {
				writer.dest.writeVarQty(0);
				writer.dest.writeVarQty(val_bi.not());
			}
		}
		else
			writeValue(writer, ((Number)val).longValue());
	}

	void writeValue(UserpWriter writer, long val) throws IOException {
		if (val >= 0)
			writer.dest.writeVarQty(val+1);
		else {
			writer.dest.writeVarQty(~val);
		}
	}

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		reader.src.readVarQty();
		int bits= reader.src.getScalarBitLen();
		if (bits < 2 && reader.src.scalarAsInt() == 0) {
			reader.src.readVarQty();
			bits= reader.src.getScalarBitLen();
			if (bits <= 63) {
				vr.i64= ~reader.src.scalarAsLong();
				vr.sto= vr.getTypeForBitLen(bits);
			}
			else {
				vr.obj= reader.src.scalarAsBigInt().not();
				vr.sto= vr.sto.OBJECT;
			}
		}
		else {
			if (bits <= 63) {
				vr.i64= reader.src.scalarAsLong()-1;
				vr.obj= vr.getTypeForBitLen(bits);
			}
			else {
				vr.obj= reader.src.scalarAsBigInt().subtract(BigInteger.ONE);
				vr.sto= vr.sto.OBJECT;
			}
		}
	}

	void skipValue(UserpReader reader) throws IOException {
		reader.src.readVarQty();
		if (reader.src.getScalarBitLen() < 2 && reader.src.scalarAsInt() == 0)
			reader.src.readVarQty();
	}
}

class EnumCodec_inf extends ScalarCodec {
	int bitLen;
	BigInteger max= null;

	protected EnumCodec_inf(ScalarType t, CodecDescriptor cd) {
		super(t, cd);
		BigInteger range= t.getValueCount();
		if (range == UserpType.INF)
			bitLen= -1;
		else {
			max= t.getValueCount().subtract(BigInteger.ONE);
			bitLen= max.bitLength();
		}
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof BigInteger) {
			BigInteger val_bi= (BigInteger) val;
			if (bitLen != -1 && val_bi.compareTo(max) > 0)
				throw new IllegalArgumentException("Scalar value out of bounds: "+val+" > "+max);
			writer.dest.writeQty(val_bi, bitLen);
		}
		else
			writeValue(writer, ((Number)val).longValue());
	}
	void writeValue(UserpWriter writer, long val) throws IOException {
		// no range check, because we determined that the value could be greater than 63 bits
		writer.dest.writeQty(val, bitLen);
	}
	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		reader.src.readQty(bitLen);
		if (reader.src.scalar != null) {
			if (bitLen != -1 && reader.src.scalar.compareTo(max) > 0)
				throw new UserpProtocolException("Scalar value out of bounds: "+reader.src.scalar+" > "+max);
			vr.obj= reader.src.scalar;
			vr.sto= vr.sto.OBJECT;
		}
		else {
			// no range check, because we determined that the value could be greater than 63 bits
			vr.i64= reader.src.scalar64;
			vr.sto= vr.getTypeForBitLen(Util.getBitLength(vr.i64));
		}
	}
	void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

class EnumCodec_63 extends EnumCodec_inf {
	ValRegister.StoType stoClass;
	long max;

	protected EnumCodec_63(ScalarType t, CodecDescriptor cd) {
		super(t, cd);
		max= t.getValueCount().subtract(BigInteger.ONE).longValue();
		stoClass= ValRegister.getTypeForBitLen(Util.getBitLength(max));
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof BigInteger) {
			BigInteger val_bi= (BigInteger) val;
			if (val_bi.bitLength() > 63 || val_bi.longValue() > max)
				throw new IllegalArgumentException("Scalar value out of bounds: "+val+" > "+max);
			writer.dest.writeQty(val_bi.longValue(), bitLen);
		}
		else
			writeValue(writer, ((Number)val).longValue());
	}

	void writeValue(UserpWriter writer, long val) throws IOException {
		if (val > max)
			throw new IllegalArgumentException("Scalar value out of bounds: "+val+" > "+max);
		writer.dest.writeQty(val, bitLen);
	}

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		reader.src.readQty(bitLen);
		long val= reader.src.scalarAsLong();
		if (val > max)
			throw new UserpProtocolException("Scalar value out of bounds: "+val+" > "+max);
		vr.i64= val;
		vr.sto= stoClass;
	}

	void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

/** A class that reads and writes values as a union would.
 * The sole purpose of this class is to prevent TAny's and Union's codecs from
 * having copy&pasted read/write/skip methods.
 */
abstract class UnionishCodec extends Codec {
	public UnionishCodec(CodecDescriptor cd) {
		super(cd);
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		TypedData td= (TypedData) val;
		selectType(writer, writer.getCodecFor(td.dataType));
		writer.write(td.data);
	}

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		inspectUnion(reader);
		UserpType t= reader.getType();
		vr.obj= new TypedData(t, reader.readValue());
		vr.sto= vr.sto.OBJECT;
	}

	void skipValue(UserpReader reader) throws IOException {
		inspectUnion(reader);
		reader.skipValue();
	}
}

class TypeAnyCodec extends UnionishCodec {
	TypeAnyCodec() {
		super(TAnyCodecDescriptor.INSTANCE);
	}

	public UserpType getType() {
		return Userp.TAny;
	}

	void selectType(UserpWriter writer, CodecDescriptor whichUnionMember) throws IOException {
		writer.checkWhetherCodecDefined(whichUnionMember);
		writer.dest.writeType(whichUnionMember);
		writer.nodeCodec= whichUnionMember.codec;
		writer.nodeCodecOverride= whichUnionMember.userCodec;
	}

	int inspectUnion(UserpReader reader) throws IOException {
		reader.nodeCodec= reader.src.readType();
		reader.nodeCodecOverride= reader.nodeCodec.descriptor.userCodec;
		return -1;
	}
}

abstract class UnionCodec extends UnionishCodec {
	UnionType type;
	Codec[] members;
	int selectorBitLen;
	boolean bitpack;
	boolean[] memberInlineFlags;

	protected UnionCodec(UnionType t, UnionCodecDescriptor cd) {
		super(cd);
		this.type= t;
		this.members= new Codec[cd.members.length];
		this.bitpack= cd.bitpack;
		this.memberInlineFlags= cd.inlineMember;
	}

	public UserpType getType() {
		return type;
	}

	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		selectType(writer, getIdxOf(type, whichUnionMember));
	}

	void selectType(UserpWriter writer, CodecDescriptor whichUnionMember) throws IOException {
		selectType(writer, getIdxOf(type, whichUnionMember.type));
	}

	static protected int getIdxOf(UnionType type, UserpType member) {
		int result= type.def.getMemberIdx(member);
		if (result == -1)
			throw new RuntimeException("Type "+member+" is not a member of "+type);
		return result;
	}

	protected abstract void selectType(UserpWriter writer, int memberIdx) throws IOException;
}

class UnionCodec_bi extends UnionCodec {
	BigInteger[] offsets;
	BigInteger max;

	public UnionCodec_bi(UnionType t, UnionCodecDescriptor cd, BigInteger scalarRange, BigInteger[] offsets) {
		super(t, cd);
		this.offsets= offsets;
		if (scalarRange == UserpType.INF)
			selectorBitLen= -1;
		else {
			max= scalarRange.subtract(BigInteger.ONE);
			selectorBitLen= max.bitLength();
		}
	}

	protected void selectType(UserpWriter writer, int memberIdx) throws IOException {
		if (bitpack)
			writer.dest.enableBitpack(true);
		if (memberInlineFlags[memberIdx])
			writer.dest.applyScalarTransform(offsets[memberIdx], selectorBitLen);
		else
			writer.dest.writeQty(offsets[memberIdx], selectorBitLen);
		writer.nodeCodec= members[memberIdx];
		writer.nodeCodecOverride= members[memberIdx].descriptor.userCodec;
	}

	int inspectUnion(UserpReader reader) throws IOException {
		if (bitpack)
			reader.src.enableBitpack(true);
		reader.src.readQty(selectorBitLen);
		BigInteger selector= reader.src.scalarAsBigInt();
		int memberIdx= findRangeIdxOf(offsets, selector);
		if (memberInlineFlags[memberIdx])
			reader.src.setNextQty(selector.subtract(offsets[memberIdx]));
		reader.nodeCodec= members[memberIdx];
		reader.nodeCodecOverride= members[memberIdx].descriptor.userCodec;
		return memberIdx;
	}

	static int findRangeIdxOf(BigInteger[] offsets, BigInteger val) {
		int min= 0, max= offsets.length-1, mid= 0;
		while (min < max) {
			mid= (min+max+1) >> 1;
			if (val.compareTo(offsets[mid]) < 0) max= mid-1;
			else min= mid;
		}
		return max;
	}
}

class UnionCodec_63 extends UnionCodec {
	long[] offsets;
	long max;

	public UnionCodec_63(UnionType t, UnionCodecDescriptor cd, long range, BigInteger[] offsets) {
		super(t, cd);
		this.offsets= new long[offsets.length];
		for (int i=0; i<offsets.length; i++)
			this.offsets[i]= offsets[i].longValue();
		max= range-1;
		selectorBitLen= Util.getBitLength(max);
	}

	protected void selectType(UserpWriter writer, int memberIdx) throws IOException {
		if (bitpack)
			writer.dest.enableBitpack(true);
		if (memberInlineFlags[memberIdx])
			writer.dest.applyScalarTransform(offsets[memberIdx], selectorBitLen);
		else
			writer.dest.writeQty(offsets[memberIdx], selectorBitLen);
		writer.nodeCodec= members[memberIdx];
		writer.nodeCodecOverride= members[memberIdx].descriptor.userCodec;
	}

	int inspectUnion(UserpReader reader) throws IOException {
		if (bitpack)
			reader.src.enableBitpack(true);
		reader.src.readQty(selectorBitLen);
		long selector= reader.src.scalarAsLong();
		int memberIdx= findRangeIdxOf(offsets, selector);
		if (memberInlineFlags[memberIdx])
			reader.src.setNextQty(selector - offsets[memberIdx]);
		reader.nodeCodec= members[memberIdx];
		reader.nodeCodecOverride= members[memberIdx].descriptor.userCodec;
		return memberIdx;
	}

	static int findRangeIdxOf(long[] offsets, long val) {
		int min= 0, max= offsets.length-1, mid= 0;
		while (min < max) {
			mid= (min+max+1) >> 1;
			if (val < offsets[mid]) max= mid-1;
			else min= mid;
		}
		return max;
	}
}

abstract class TupleCodec extends Codec {
	TupleType type;
	TupleCoding encMode;
	Codec elemCodec;
	Codec[] elemCodecs;

	protected TupleCodec(TupleType t, CodecDescriptor cd, TupleCoding encMode) {
		super(cd);
		this.type= t;
		this.encMode= encMode;
		if (t instanceof RecordType)
			elemCodecs= new Codec[t.getElemCount()];
	}

	public UserpType getType() {
		return type;
	}

	Codec elemCodec(int idx) {
		return (elemCodecs != null)? elemCodecs[idx] : elemCodec;
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof Collection) {
			Collection elems= (Collection) val;
			writer.beginTuple(elems.size());
			for (Object elemData: elems)
				writer.write(elemData);
			writer.endTuple();
		}
		else {
			Object[] elems= (Object[]) val;
			writer.beginTuple(elems.length);
			for (Object elemData: elems)
				writer.write(elemData);
			writer.endTuple();
		}
	}

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		int elemCount= reader.inspectTuple();
//		Class stoClass= reader.tupleType.getNativeStoragePref();
//		Object result;
//		if (stoClass != null && stoClass != Object[].class) {
//			result= Array.newInstance(stoClass.getComponentType(), count);
//			for (int i=0; i<count; i++, reader.nextElem())
//				Array.set(result, i, reader.readValue());
//		}
		if (elemCount == -1) {
			LinkedList result= new LinkedList();
			while (reader.getType() != null)
				result.add(reader.readValue());
			vr.obj= result.toArray();
		}
		else {
			Object[] result= new Object[elemCount];
			for (int i=0; i<elemCount; i++)
				result[i]= reader.readValue();
			vr.obj= result;
		}
		vr.sto= vr.sto.OBJECT;
		reader.closeTuple();
	}

	void skipValue(UserpReader reader) throws IOException {
		reader.inspectTuple();
		while (reader.getType() != null)
			reader.skipValue();
		reader.closeTuple();
	}

	int readElemCount(UserpReader reader) throws IOException {
		int count= type.getElemCount();
		if (count < 0) {
			reader.src.readVarQty();
			count= reader.src.scalarAsInt();
		}
		return count;
	}

	abstract void nextElement(UserpReader reader) throws IOException;
	abstract void closeTuple(UserpReader reader) throws IOException;
	abstract void nextElement(UserpWriter writer) throws IOException;
	abstract void endTuple(UserpWriter writer) throws IOException;
}

class PackedTupleCodec extends TupleCodec {
	protected PackedTupleCodec(TupleType t, CodecDescriptor cd, TupleCoding encMode) {
		super(t, cd, encMode);
	}

	int inspectTuple(UserpReader reader) throws IOException {
		reader.tupleState.tupleCodec= this;
		reader.tupleState.elemCount= readElemCount(reader);
		reader.tupleState.elemIdx= 0;
		reader.nodeCodec= elemCodec(0);
		reader.nodeCodecOverride= elemCodec(0).descriptor.userCodec;
		if (encMode == TupleCoding.BITPACK)
			reader.src.enableBitpack(true);
		reader.tupleState.bitpack= reader.src.bitpack;
		return reader.tupleState.elemCount;
	}

	void nextElement(UserpReader reader) throws IOException {
		if (++reader.tupleState.elemIdx < reader.tupleState.elemCount) {
			reader.nodeCodec= elemCodec(reader.tupleState.elemIdx);
			reader.nodeCodecOverride= reader.nodeCodec.descriptor.userCodec;
		}
		else {
			reader.nodeCodec= TupleEndSentinelCodec.INSTANCE;
			reader.nodeCodecOverride= null;
		}
		reader.src.enableBitpack(reader.tupleState.bitpack);
	}

	void closeTuple(UserpReader reader) throws IOException {
		while (reader.tupleState.elemIdx < reader.tupleState.elemCount)
			reader.skipValue();
		assert(reader.nodeCodec == TupleEndSentinelCodec.INSTANCE);
	}

	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		int fixedCount= type.getElemCount();
		if (fixedCount >= 0)
			writer.tupleState.elemCount= fixedCount;
		else {
			if (elemCount < 0)
				throw new RuntimeException(type.getDisplayName()+".beginTuple(int) requires an element count");
			writer.dest.writeVarQty(elemCount);
			writer.tupleState.elemCount= elemCount;
		}
		writer.tupleState.tupleCodec= this;
		writer.tupleState.elemIdx= 0;
		writer.nodeCodec= null;
		writer.nodeCodecOverride= null;
		if (encMode == TupleCoding.BITPACK)
			writer.dest.enableBitpack(true);
		writer.tupleState.bitpack= writer.dest.bitpack;
	}

	void nextElement(UserpWriter writer) throws IOException {
		if (writer.tupleState.elemIdx < writer.tupleState.elemCount) {
			writer.nodeCodec= elemCodec(writer.tupleState.elemIdx++);
			writer.nodeCodecOverride= writer.nodeCodec.descriptor.userCodec;
		}
		else {
			writer.nodeCodec= TupleEndSentinelCodec.INSTANCE;
			writer.nodeCodecOverride= null;
		}
		writer.dest.enableBitpack(writer.tupleState.bitpack);
	}

	void endTuple(UserpWriter writer) throws IOException {
		if (writer.tupleState.elemIdx != writer.tupleState.elemCount)
			throw new RuntimeException(type.getDisplayName()+" has "+writer.tupleState.elemCount+" elements, but only "+writer.tupleState.elemIdx+" were written.");
	}
}

class ArrayOfByteCodec extends PackedTupleCodec {
	protected ArrayOfByteCodec(TupleType t, CodecDescriptor cd, TupleCoding encMode) {
		super(t, cd, encMode);
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		byte[] valArray= (byte[]) val;
		int fixedCount= type.getElemCount();
		if (fixedCount < 0)
			writer.dest.writeVarQty(valArray.length);
		writer.dest.writeBits(valArray, 0, valArray.length<<3);
	}

	void readValue(UserpReader reader, ValRegister vr) throws IOException {
		int elemCount= readElemCount(reader);
		byte[] result= new byte[elemCount];
		// no need to enable bitpacking, because every element is 8 bits
		reader.src.readBits(result, 0, elemCount<<3);
		vr.obj= result;
		vr.sto= vr.sto.OBJECT;
	}

	void skipValue(UserpReader reader) throws IOException {
		int elemCount= readElemCount(reader);
		// no need to enable bitpacking, because every element is 8 bits
		reader.src.skipBits(elemCount<<3);
	}
}

class NativeTypeConverter implements CustomCodec {
	ValRegister.StoType specifiedStorage;

	public NativeTypeConverter(ValRegister.StoType specifiedStorage) {
		this.specifiedStorage= specifiedStorage;
	}

	public void encode(UserpWriter dest, ValRegister.StoType type, long i64val, Object oVal) throws IOException {
		if (type != ValRegister.StoType.OBJECT)
			dest.write(i64val);
		else
			dest.write(oVal);
	}

	public void decode(UserpReader src, ValRegister val) throws IOException {
		src.readValue();
		// Heees Eeeeeee-Viiiiiil  Eeevil is his one and on-ly name....
		val.sto= specifiedStorage;
	}
}

class SymbolConverter implements CustomCodec {
	public void encode(UserpWriter dest, ValRegister.StoType type, long i64val, Object oVal) throws IOException {
		dest.write(((Symbol)oVal).data);
	}

	public void decode(UserpReader src, ValRegister val) throws IOException {
		val.obj= new Symbol((byte[])src.readValue());
	}

	static final SymbolConverter INSTANCE= new SymbolConverter();
}

class NullConverter implements CustomCodec {
	public void encode(UserpWriter dest, ValRegister.StoType type, long i64val, Object oVal) throws IOException {
	}

	public void decode(UserpReader src, ValRegister val) throws IOException {
		val.obj= null;
		val.sto= ValRegister.StoType.OBJECT;
	}

	static final NullConverter INSTANCE= new NullConverter();
}
