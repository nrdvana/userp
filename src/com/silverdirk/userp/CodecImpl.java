package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.*;
import java.io.*;
import com.silverdirk.userp.ValRegister.StoType;
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
public abstract class CodecImpl {
	Codec descriptor;

	protected CodecImpl(Codec cd) {
		descriptor= cd;
	}

	final Codec getDescriptor() {
		return descriptor;
	}

	final UserpType getType() {
		return descriptor.type;
	}

	public abstract void writeValue(UserpWriter writer, ValRegister.StoType type, long i64val, Object oVal) throws IOException;
	void selectType(UserpWriter writer, UserpType memberType) throws IOException {
		writer.checkWhetherTypeKnown(memberType);
		selectType(writer, writer.codecSet.getCodecFor(memberType));
	}
	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		throw new RuntimeException("Operation SelectType not applicable to type "+getType());
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new RuntimeException("Operation BeginTuple not applicable to type "+getType());
	}

	public abstract void readValue(UserpReader reader, ValRegister vr) throws IOException;
	public abstract void skipValue(UserpReader reader) throws IOException;
	int inspectUnion(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectUnion not applicable to type "+getType());
	}
	int inspectTuple(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectTuple not applicable to type "+getType());
	}
}

abstract class ScalarImpl extends CodecImpl {
	ScalarType type;

	protected ScalarImpl(ScalarType t, Codec cd) {
		super(cd);
		this.type= t;
	}
}

class IntegerImpl extends ScalarImpl {
	protected IntegerImpl(ScalarType t, Codec cd) {
		super(t, cd);
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		if (type == StoType.OBJECT) {
			if (val_o instanceof BigInteger) {
				BigInteger val_bi= (BigInteger) val_o;
				if (val_bi.signum() >= 0)
					writer.dest.writeVarQty(val_bi.add(BigInteger.ONE));
				else {
					writer.dest.writeVarQty(0);
					writer.dest.writeVarQty(val_bi.not());
				}
				return;
			}
			else
				val_i64= ((Number)val_o).longValue();
		}
		if (val_i64 >= 0)
			writer.dest.writeVarQty(val_i64+1);
		else
			writer.dest.writeVarQty(~val_i64);
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
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

	public void skipValue(UserpReader reader) throws IOException {
		reader.src.readVarQty();
		if (reader.src.getScalarBitLen() < 2 && reader.src.scalarAsInt() == 0)
			reader.src.readVarQty();
	}
}

class EnumImpl_inf extends ScalarImpl {
	int bitLen;
	BigInteger max= null;

	protected EnumImpl_inf(ScalarType t, Codec cd) {
		super(t, cd);
		BigInteger range= t.getValueCount();
		if (range == UserpType.INF)
			bitLen= -1;
		else {
			max= t.getValueCount().subtract(BigInteger.ONE);
			bitLen= max.bitLength();
		}
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		if (type == StoType.OBJECT) {
			if (val_o instanceof BigInteger) {
				BigInteger val_bi= (BigInteger) val_o;
				if (bitLen != -1 && val_bi.compareTo(max) > 0)
					throw new IllegalArgumentException("Scalar value out of bounds: "+val_o+" > "+max);
				writer.dest.writeQty(val_bi, bitLen);
				return;
			}
			else
				val_i64= ((Number)val_o).longValue();
		}
		// no range check, because we determined that max is larger than long
		writer.dest.writeQty(val_i64, bitLen);
	}
	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
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
	public void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

class EnumImpl_63 extends EnumImpl_inf {
	ValRegister.StoType stoClass;
	long max;

	protected EnumImpl_63(ScalarType t, Codec cd) {
		super(t, cd);
		max= t.getValueCount().subtract(BigInteger.ONE).longValue();
		stoClass= ValRegister.getTypeForBitLen(Util.getBitLength(max));
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		if (type == StoType.OBJECT) {
			if (val_o instanceof BigInteger) {
				BigInteger val_bi= (BigInteger) val_o;
				if (val_bi.bitLength() > 63 || val_bi.longValue() > max)
					throw new IllegalArgumentException("Scalar value out of bounds: "+val_o+" > "+max);
				val_i64= val_bi.longValue();
			}
			else
				val_i64= ((Number)val_o).longValue();
		}
		if (val_i64 > max)
			throw new IllegalArgumentException("Scalar value out of bounds: "+val_i64+" > "+max);
		writer.dest.writeQty(val_i64, bitLen);
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
		reader.src.readQty(bitLen);
		long val= reader.src.scalarAsLong();
		if (val > max)
			throw new UserpProtocolException("Scalar value out of bounds: "+val+" > "+max);
		vr.i64= val;
		vr.sto= stoClass;
	}

	public void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

/** A class that reads and writes values as a union would.
 * The sole purpose of this class is to prevent TAny's and Union's codecs from
 * having copy&pasted read/write/skip methods.
 */
abstract class UnionishImpl extends CodecImpl {
	public UnionishImpl(Codec cd) {
		super(cd);
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		if (type != StoType.OBJECT)
			throw new ClassCastException("Cannot cast from "+Util.getReadableClassName(type.javaCls)+" to TypedData");
		TypedData td= (TypedData) val_o;
		selectType(writer, writer.getCodecFor(td.dataType));
		writer.write(td.data);
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
		inspectUnion(reader);
		UserpType t= reader.getType();
		vr.obj= new TypedData(t, reader.readValue());
		vr.sto= vr.sto.OBJECT;
	}

	public void skipValue(UserpReader reader) throws IOException {
		inspectUnion(reader);
		reader.skipValue();
	}
}

class TypeAnyImpl extends UnionishImpl {
	TypeAnyImpl() {
		super(TAnyCodec.INSTANCE);
	}

	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		writer.checkWhetherCodecDefined(whichUnionMember);
		writer.dest.writeType(whichUnionMember);
		writer.nodeCodec= whichUnionMember;
	}

	int inspectUnion(UserpReader reader) throws IOException {
		reader.nodeCodec= reader.src.readType();
		return -1;
	}
}

abstract class UnionImpl extends UnionishImpl {
	UnionType type;
	Codec[] members;
	int selectorBitLen;
	boolean bitpack;
	boolean[] memberInlineFlags;

	protected UnionImpl(UnionType t, UnionCodec cd) {
		super(cd);
		this.type= t;
		this.members= cd.members;
		this.bitpack= cd.bitpack;
		this.memberInlineFlags= cd.inlineMember;
	}

	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		selectType(writer, getIdxOf(type, whichUnionMember));
	}

	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
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

class UnionImpl_bi extends UnionImpl {
	BigInteger[] offsets;
	BigInteger max;

	public UnionImpl_bi(UnionType t, UnionCodec cd, BigInteger scalarRange, BigInteger[] offsets) {
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

class UnionImpl_63 extends UnionImpl {
	long[] offsets;
	long max;

	public UnionImpl_63(UnionType t, UnionCodec cd, long range, BigInteger[] offsets) {
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

abstract class TupleImpl extends CodecImpl {
	TupleType type;
	TupleCoding encMode;
	Codec elemCodec;
	Codec[] elemCodecs;

	/** Special constructor for the subclass SentinelCodec
	 */
	protected TupleImpl(SentinelCodec c) {
		super(c);
	}

	protected TupleImpl(TupleType t, Codec cd, TupleCoding encMode) {
		super(cd);
		this.type= t;
		this.encMode= encMode;
		if (cd instanceof RecordCodec)
			elemCodecs= ((RecordCodec)cd).elemTypes;
		else
			elemCodec= ((ArrayCodec)cd).elemType;
	}

	Codec elemCodec(int idx) {
		return (elemCodecs != null)? elemCodecs[idx] : elemCodec;
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		if (type != StoType.OBJECT)
			throw new ClassCastException("Cannot cast from "+Util.getReadableClassName(type.javaCls)+" to Object[] or Collection");
		if (val_o instanceof Collection) {
			Collection elems= (Collection) val_o;
			writer.beginTuple(elems.size());
			for (Object elemData: elems)
				writer.write(elemData);
			writer.endTuple();
		}
		else {
			Object[] elems= (Object[]) val_o;
			writer.beginTuple(elems.length);
			for (Object elemData: elems)
				writer.write(elemData);
			writer.endTuple();
		}
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
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

	public void skipValue(UserpReader reader) throws IOException {
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

class PackedTupleImpl extends TupleImpl {
	protected PackedTupleImpl(TupleType t, Codec cd, TupleCoding encMode) {
		super(t, cd, encMode);
	}

	int inspectTuple(UserpReader reader) throws IOException {
		reader.tupleState.TupleImpl= this;
		reader.tupleState.elemCount= readElemCount(reader);
		reader.tupleState.elemIdx= 0;
		reader.nodeCodec= elemCodec(0);
		if (encMode == TupleCoding.BITPACK)
			reader.src.enableBitpack(true);
		reader.tupleState.bitpack= reader.src.bitpack;
		return reader.tupleState.elemCount;
	}

	void nextElement(UserpReader reader) throws IOException {
		reader.nodeCodec= (++reader.tupleState.elemIdx >= reader.tupleState.elemCount)?
			TupleEndSentinelCodec.INSTANCE.descriptor : elemCodec(reader.tupleState.elemIdx);
		reader.src.enableBitpack(reader.tupleState.bitpack);
	}

	void closeTuple(UserpReader reader) throws IOException {
		while (reader.tupleState.elemIdx < reader.tupleState.elemCount)
			reader.skipValue();
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
		writer.tupleState.tupleImpl= this;
		writer.tupleState.elemIdx= 0;
		writer.nodeCodec= null;
		if (encMode == TupleCoding.BITPACK)
			writer.dest.enableBitpack(true);
		writer.tupleState.bitpack= writer.dest.bitpack;
	}

	void nextElement(UserpWriter writer) throws IOException {
		if (writer.tupleState.elemIdx < writer.tupleState.elemCount)
			writer.nodeCodec= elemCodec(writer.tupleState.elemIdx++);
		else
			writer.nodeCodec= TupleEndSentinelCodec.INSTANCE.descriptor;
		writer.dest.enableBitpack(writer.tupleState.bitpack);
	}

	void endTuple(UserpWriter writer) throws IOException {
		if (writer.tupleState.elemIdx != writer.tupleState.elemCount)
			throw new RuntimeException(type.getDisplayName()+" has "+writer.tupleState.elemCount+" elements, but only "+writer.tupleState.elemIdx+" were written.");
	}
}

class ArrayOfByteImpl extends PackedTupleImpl {
	protected ArrayOfByteImpl(TupleType t, Codec cd, TupleCoding encMode) {
		super(t, cd, encMode);
	}

	public void writeValue(UserpWriter writer, ValRegister.StoType stoType, long val_i64, Object val_o) throws IOException {
		if (stoType != StoType.OBJECT)
			throw new ClassCastException("Cannot cast from "+Util.getReadableClassName(stoType.javaCls)+" to byte[]");
		byte[] valArray= (byte[]) val_o;
		int fixedCount= type.getElemCount();
		if (fixedCount < 0)
			writer.dest.writeVarQty(valArray.length);
		writer.dest.writeBits(valArray, 0, valArray.length<<3);
	}

	public void readValue(UserpReader reader, ValRegister vr) throws IOException {
		int elemCount= readElemCount(reader);
		byte[] result= new byte[elemCount];
		// no need to enable bitpacking, because every element is 8 bits
		reader.src.readBits(result, 0, elemCount<<3);
		vr.obj= result;
		vr.sto= vr.sto.OBJECT;
	}

	public void skipValue(UserpReader reader) throws IOException {
		int elemCount= readElemCount(reader);
		// no need to enable bitpacking, because every element is 8 bits
		reader.src.skipBits(elemCount<<3);
	}
}

class NativeTypeConverter implements CodecOverride {
	ValRegister.StoType specifiedStorage;

	public NativeTypeConverter(ValRegister.StoType specifiedStorage) {
		this.specifiedStorage= specifiedStorage;
	}

	public void writeValue(CodecImpl parent, UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		parent.writeValue(writer, type, val_i64, val_o);
	}

	public void readValue(CodecImpl parent, UserpReader reader, ValRegister val) throws IOException {
		parent.readValue(reader, val);
		val.sto= specifiedStorage;
	}
}

class SymbolConverter implements CodecOverride {
	public void writeValue(CodecImpl parent, UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
		parent.writeValue(writer, type, val_i64, ((Symbol)val_o).data);
	}

	public void readValue(CodecImpl parent, UserpReader reader, ValRegister val) throws IOException {
		parent.readValue(reader, val);
		val.obj= new Symbol((byte[])val.obj);
	}

	static final SymbolConverter INSTANCE= new SymbolConverter();
}

class NullConverter implements CodecOverride {
	public void writeValue(CodecImpl parent, UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
	}

	public void readValue(CodecImpl parent, UserpReader src, ValRegister val) throws IOException {
		val.obj= null;
		val.sto= ValRegister.StoType.OBJECT;
	}

	static final NullConverter INSTANCE= new NullConverter();
}
