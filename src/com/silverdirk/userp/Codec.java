package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.*;
import java.io.*;
import com.silverdirk.userp.RecordType.Field;
import com.silverdirk.userp.EnumType.Range;
import com.silverdirk.userp.ValRegister.StoType;
import com.silverdirk.userp.UserpWriter.Action;
import com.silverdirk.userp.UserpWriter.Event;

abstract class Codec {
	UserpType type;
	CodecOverride implOverride;
	CodecImpl impl;
	protected BigInteger scalarRange;
	int initStatus;
	int encoderTypeCode= -1;

	static final int
		ISTAT_NEED_DESC_REFS= 0,
		ISTAT_NEED_IMPL= 1,
		ISTAT_READY= 3;

	static Codec forType(UserpType t) {
		return t.createCodec();
	}

	static Codec forType(UnionType t, boolean bitpack, boolean[] inlineMember) {
		return new UnionCodec(t, bitpack, inlineMember);
	}

	static Codec forType(TupleType t, TupleCoding encoding) {
		if (t instanceof ArrayType)
			return new ArrayCodec((ArrayType)t, encoding);
		else
			return new RecordCodec((RecordType)t, encoding);
	}

	static Codec forCodec(CodecImpl c) {
		return c.getDescriptor();
	}

	public boolean equals(Object other) {
		return (getClass() == other.getClass()) && type.equals(((Codec)other).type);
	}

	static final BigInteger SCALAR_IN_PROGRESS;
	public BigInteger getScalarRange() {
		if (scalarRange == SCALAR_IN_PROGRESS)
			throw new UninitializedTypeException(type, "getScalarRange");
		if (scalarRange == null) {
			scalarRange= SCALAR_IN_PROGRESS;
			initScalarRange();
		}
		return scalarRange;
	}

	abstract protected void initScalarRange();

	static final CodecImpl IMPL_IN_PROGRESS;
	public void resolveCodec() {
		if (initStatus <= ISTAT_NEED_IMPL) {
			if (impl == IMPL_IN_PROGRESS)
				throw new UninitializedTypeException(type, "getCodec");
			if (impl == null) {
				impl= IMPL_IN_PROGRESS;
				initCodec();
				initStatus= ISTAT_READY;
			}
		}
	}

	abstract protected void initCodec();

	static Codec deserialize(UserpReader src) throws IOException {
		Codec result;
		int elemCnt= src.inspectTuple();
		assert(elemCnt == 2);
		Symbol name= (Symbol) src.readValue();
		switch (src.inspectUnion()) {
		case 0: result= new IntegerCodec(name, src); break;
		case 1: result= new UnionCodec(name, src); break;
		case 2: result= new ArrayCodec(name, src); break;
		case 3: result= new RecordCodec(name, src); break;
		case 4: result= new EnumCodec(name, src); break;
		default:
			throw new Error("Bug");
		}
		src.closeTuple();
		return result;
	}

	abstract void resolveDescriptorRefs(CodecBuilder cb);
	abstract void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) throws UserpProtocolException;

	final void serialize(UserpWriter writer) throws IOException {
		writer.beginTuple();
		writer.write(type.getName());
		serializeFields(writer);
		writer.endTuple();
	}

	abstract protected void serializeFields(UserpWriter writer) throws IOException;

	static final Codec
		CTypeSpec, CEnumSpec;
	static final UserpType
		TEnumSize, TEnumSpec, TIntSpec, TUnionSpec, TArraySpec, TRecordSpec,
		TInfinities, TEnumSpecRange;
	static {
		IMPL_IN_PROGRESS= new TypeAnyImpl();
		SCALAR_IN_PROGRESS= new BigInteger("-1"); // need to use a constructor to ensure that we have a unique object
		TIntSpec= Userp.TNull.cloneAs("TIntSpec");
		TEnumSize= Userp.TWhole.cloneAs("TEnumSize");
		UserpType TWholeArray= new ArrayType("TWholeArray").init(Userp.TWhole);
		TUnionSpec= new RecordType("TUnionSpec").init(new Field[]{
			new Field("Bitpack", Userp.TBool),
			new Field("Members", TWholeArray),
			new Field("ScalarInline", Userp.TBoolArray)
		});
		UserpType TTupleCoding= new EnumType("TTupleCoding").init(new Object[] {"Indefinite", "Index", "Pack", "Bitpack"});
		UserpType TArrayLen= new EnumType("TArrayLen").init(new Object[] {"VarLen", new Range(Userp.TWhole, 0, Range.INF)});
		TArraySpec= new RecordType("TArraySpec").init(new Field[] {
			new Field("EncMode", TTupleCoding),
			new Field("ElemType", Userp.TWhole),
			new Field("Len", TArrayLen),
		});
		UserpType TField= new RecordType("TField").init(new Field[] {
			new Field("Name", Userp.TSymbol),
			new Field("Type", Userp.TWhole),
		});
		TRecordSpec= new RecordType("TRecordSpec").init(new Field[] {
			new Field("EncMode", TTupleCoding),
			new Field("Fields", new ArrayType(Symbol.NIL).init(TField)),
		});
		UserpType TSpecUnion= new UnionType("SpecUnion")
			.init(new UserpType[] {TIntSpec, TUnionSpec, TArraySpec, TRecordSpec, TEnumSize})
			.setEncParams(false, new boolean[] {false, true, true, true, true});
		UserpType TTypeSpec= new RecordType("TTypeSpec").init(new Field[] {
			new Field("Name", Userp.TSymbol),
			new Field("Spec", TSpecUnion)
		});

		// Enum ranges can extend to +INF or -INF
		TInfinities= new EnumType(Symbol.NIL).init(new Object[] {"+INF", "-INF"});
		// Enum ranges are a scalar type, and a 'to' and 'from' value
		TEnumSpecRange= new RecordType("TEnumRange").init(new Field[] {
			new Field("To",
				new UnionType(Symbol.NIL).init(new UserpType[] {TInfinities, Userp.TInt}).setEncParams(false, new boolean[] {true, false})
			),
			new Field("From", Userp.TInt),
			new Field("Type", Userp.TWhole)
		}).setEncParam(TupleCoding.PACK);
		TEnumSpecRange.setCustomCodec(new EnumRangeConverter());

		// Each element of an enum spec is either a Range, Value, or Symbol
		UserpType TEnumSpecElement= new UnionType("TEnumSpec").init(
			new UserpType[] {Userp.TAny, TEnumSpecRange, Userp.TSymbol}
		).setEncParams(false, new boolean[] {false, true, true});
		TEnumSpecElement.setCustomCodec(new EnumSpecElementConverter());

		// The enum spec is an array of spec elements
		TEnumSpec= new ArrayType(Symbol.NIL).init(TEnumSpecElement);

		// Build codecs that will be used for encoding and decoding type tables.
		CodecBuilder cb= new CodecBuilder(true);
		CTypeSpec= cb.getCodecFor(TTypeSpec);
		CEnumSpec= cb.getCodecFor(TEnumSpec);
	}

	/** Automatic conversion to/from class EnumType.Range
	 */
	static class EnumRangeConverter implements CodecOverride {
		public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
			Range r= (Range) val_o;
			writer.beginTuple();
			if (r.to instanceof UserpType.InfFlag)
				writer.select(TInfinities).write(r.to == Range.INF? 0 : 1);
			else
				writer.select(Userp.TInt).write(r.to);
			writer.write(r.from);
			writer.dest.writeType(r.domain);
		}
		public void loadValue(UserpReader reader) throws IOException {
			BigInteger from, to;
			reader.inspectTuple();
			// read the "to" field, which is a union of "+Inf/-Inf" enum and a BigInteger.
			if (reader.inspectUnion() == 0)
				to= reader.readAsInt() == 0? Range.INF : Range.NEG_INF;
			else
				to= reader.readAsBigInt();

			from= reader.readAsBigInt();

			UserpType t= reader.src.readType().type;
			if (!(t instanceof ScalarType))
				throw new UserpProtocolException("Invalid enum range domain encountered: "+t+" is not a scalar type.");

			reader.valRegister.obj= new Range((ScalarType) t, from, to);
			reader.valRegister.sto= StoType.OBJECT;
		}
	}

	/** Automatic conversion to/from the allowed classes in an element of an enum spec.
	 */
	static class EnumSpecElementConverter implements CodecOverride {
		public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException {
			if (val_o instanceof Symbol)
				writer.select(Userp.TSymbol);
			else if (val_o instanceof Range)
				writer.select(TEnumSpecRange);
			else {
				assert(val_o instanceof TypedData);
				writer.select(Userp.TAny);
			}
			writer.write(val_o);
		}
		public void loadValue(UserpReader reader) throws IOException {
			reader.inspectUnion();
			reader.loadValue();
		}
	}
}

/** Codec for type TAny.
 */
class TAnyCodec extends Codec {
	private TAnyCodec() {
		type= Userp.TAny;
		implOverride= type.getCustomCodec();
		scalarRange= UserpType.INF;
		impl= new TypeAnyImpl();
		initStatus= ISTAT_READY;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {}
	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) {}
	protected void initScalarRange() {}
	protected void initCodec() {}

	protected void serializeFields(UserpWriter writer) throws IOException {
		throw new Error("TAny not serializable");
	}

	public static final TAnyCodec INSTANCE= new TAnyCodec();
}

/** Base class used to identify scalar types: Integer, Enum
 */
abstract class ScalarCodec extends Codec {}

/**
 */
class IntegerCodec extends ScalarCodec {
	IntegerCodec(IntegerType t) {
		type= t;
		implOverride= type.getCustomCodec();
		scalarRange= ScalarType.INF;
		impl= new IntegerImpl((IntegerType)type, this);
		initStatus= ISTAT_READY;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {}

	IntegerCodec(Symbol name, UserpReader src) throws IOException {
		this(new IntegerType(name));
		src.skipValue(); // no actual value, but we have to advance the stream's current node
	}

	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) {}

	protected void serializeFields(UserpWriter writer) throws IOException {
		writer.select(TIntSpec);
		writer.write(0);
	}

	protected void initScalarRange() {}
	protected void initCodec() {}
}

class EnumCodec extends ScalarCodec {
	EnumCodec(EnumType t) {
		type= t;
		implOverride= type.getCustomCodec();
		scalarRange= ((EnumType)type).getValueCount();
		initCodec();
		initStatus= ISTAT_READY;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {}

	EnumCodec(Symbol name, UserpReader src) throws IOException {
		scalarRange= (BigInteger) src.readValue();
		if (scalarRange.equals(BigInteger.ZERO))
			scalarRange= ScalarType.INF;
		type= new EnumType(name);
		initCodec();
		initStatus= ISTAT_READY;
	}

	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) {}

	protected void serializeFields(UserpWriter writer) throws IOException {
		writer.select(TEnumSize);
		writer.write(scalarRange == UserpType.INF? BigInteger.ZERO : scalarRange);
	}

	void deserializeSpec(UserpReader reader) throws IOException {
		int specElemCount= reader.inspectTuple();
		Object[] spec= new Object[specElemCount];
		for (int i=0; i<specElemCount; i++)
			spec[i]= reader.readValue();
		reader.closeTuple();
		((EnumType)type).init(spec);

		BigInteger specValCount= ((EnumType)type).getValueCount();
		// must perform comparison both ways, because BigInteger.ZERO will think it is equal to InfFlag.
		if (!scalarRange.equals(specValCount) || !specValCount.equals(scalarRange))
			throw new UserpProtocolException("Invalid encoding of an Enum definition.  Stated size of "+scalarRange+" does not match actual size of "+specValCount);
	}

	void serializeSpec(UserpWriter writer) throws IOException {
		Object[] spec= ((EnumType)type).def.spec;
		writer.beginTuple(spec.length);
		for (Object elem: spec)
			writer.write(elem);
		writer.endTuple();
	}

	protected void initScalarRange() {}

	protected void initCodec() {
		if (scalarRange == ScalarType.INF || scalarRange.bitLength() >= 64)
			impl= new EnumImpl_inf((EnumType)type, this);
		else
			impl= new EnumImpl_63((EnumType)type, this);
	}
}

class UnionCodec extends Codec {
	boolean bitpack;
	boolean[] inlineMember;
	Codec[] members;
	int[] tempMemberTypeCode;

	public boolean equals(Object other) {
		return super.equals(other)
			&& Arrays.equals(inlineMember, ((UnionCodec)other).inlineMember)
			&& bitpack == ((UnionCodec)other).bitpack;
	}

	UnionCodec(UnionType t, boolean bitpack, boolean[] inlineMember) {
		type= t;
		implOverride= type.getCustomCodec();
		this.bitpack= bitpack;
		this.inlineMember= inlineMember;
		members= new Codec[inlineMember.length];
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {
		for (int i=0; i<members.length; i++)
			members[i]= cb.getIncompleteCodecFor(((UnionType)type).getMember(i));
		initStatus= ISTAT_NEED_IMPL;
	}

	UnionCodec(Symbol name, UserpReader reader) throws IOException {
		reader.inspectTuple();
		bitpack= reader.readAsBool();
		int memberCount= reader.inspectTuple();
		tempMemberTypeCode= new int[memberCount];
		for (int i=0; i<memberCount; i++)
			tempMemberTypeCode[i]= reader.readAsInt();
		reader.closeTuple();
		inlineMember= (boolean[]) reader.readValue();
		reader.closeTuple();

		type= new UnionType(name);
		members= new Codec[memberCount];
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) throws UserpProtocolException {
		UserpType[] typeMembers= new UserpType[members.length];
		for (int i=0; i<members.length; i++) {
			members[i]= tcm.getCodec(tempMemberTypeCode[i]);
			typeMembers[i]= members[i].type;
		}
		((UnionType)type).init(typeMembers);
		tempMemberTypeCode= null;
		initStatus= ISTAT_NEED_IMPL;
	}

	protected void serializeFields(UserpWriter dest) throws IOException {
		dest.select(TUnionSpec);
		dest.beginTuple();
		dest.write(bitpack?1:0);
		dest.beginTuple(members.length);
		for (int i=0; i<members.length; i++) {
			dest.write(members[i].encoderTypeCode);
			assert(!inlineMember[i] || members[i].encoderTypeCode < encoderTypeCode);
		}
		dest.endTuple();
		dest.write(inlineMember);
		dest.endTuple();
	}

	protected void initScalarRange() {
		initCodec(); // codec is needed for range calculation, and inits range in the process
	}

	protected void initCodec() {
		BigInteger curOffset= BigInteger.ZERO;
		BigInteger[] ofs= new BigInteger[members.length];
		for (int i= 0; i<members.length; i++) {
			ofs[i]= curOffset;
			BigInteger curRange;
			if (!inlineMember[i])
				curRange= BigInteger.ONE;
			else // else we need to know how big a scalar range to assign to this member
				curRange= members[i].getScalarRange();
			if (curRange == UserpType.INF) {
				if (i != members.length-1)
					throw new RuntimeException("sub-type "+members[i]+" is infinite, and inlined, and is not the last sub-type");
				curOffset= UserpType.INF;
			}
			else
				curOffset= curOffset.add(curRange);
		}
		scalarRange= curOffset;

		impl= (scalarRange.bitLength() >= 64 || scalarRange == UserpType.INF)?
			new UnionImpl_bi((UnionType)type, this, scalarRange, ofs)
			: new UnionImpl_63((UnionType)type, this, scalarRange.longValue(), ofs);
	}
}

abstract class TupleCodec extends Codec {}

class ArrayCodec extends TupleCodec {
	TupleCoding encoding;
	Codec elemType;
	int tempElemTypeCode;

	public boolean equals(Object other) {
		return super.equals(other)
			&& encoding == ((ArrayCodec)other).encoding;
	}

	ArrayCodec(ArrayType t, TupleCoding encoding) {
		type= t;
		implOverride= type.getCustomCodec();
		this.encoding= encoding;
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {
		elemType= cb.getIncompleteCodecFor(((ArrayType)type).getElemType(0));
		initStatus= ISTAT_NEED_IMPL;
	}

	ArrayCodec(Symbol name, UserpReader reader) throws IOException {
		reader.inspectTuple();
		encoding= TupleCoding.values()[reader.readAsInt()];
		tempElemTypeCode= reader.readAsInt();
		int lengthSpec= reader.readAsInt()-1; // value 0 is VarLen, now becomes -1
		reader.closeTuple();

		type= new ArrayType(name).init(null, lengthSpec);
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) throws UserpProtocolException {
		elemType= tcm.getCodec(tempElemTypeCode);
		((ArrayType)type).def.elemType= elemType.type;
		initStatus= ISTAT_NEED_IMPL;
	}

	protected void serializeFields(UserpWriter writer) throws IOException {
		writer.select(TArraySpec);
		writer.beginTuple();
		writer.write(encoding.ordinal());
		writer.write(elemType.encoderTypeCode);
		writer.write(((ArrayType)type).getElemCount()+1); // INF == -1, so the scalar is 0=INF,[1..N]=[0..N-1]
		writer.endTuple();
	}

	protected void initScalarRange() {
		if (encoding == TupleCoding.INDEX || encoding == TupleCoding.INDEFINITE || ((ArrayType)type).getElemCount() == -1)
			scalarRange= UserpType.INF;
		else
			scalarRange= elemType.getScalarRange();
	}

	protected void initCodec() {
		switch (encoding) {
		case PACK:
		case BITPACK:
			if (elemType instanceof ScalarCodec) {
				BigInteger elemRange= elemType.getScalarRange();
				if (elemRange.bitLength() < 10 && elemRange.intValue() <= 256 && elemRange.intValue() > 128
					&& elemType.implOverride != null
					&& elemType.implOverride.getClass() == NativeTypeConverter.class
					&& ((NativeTypeConverter)elemType.implOverride).specifiedStorage == ValRegister.StoType.BYTE)
					impl= new ArrayOfByteImpl((TupleType)type, this, encoding);
				else if (encoding == TupleCoding.BITPACK
					&& elemRange.bitLength() == 2 && elemRange.intValue() == 2
					&& elemType.implOverride != null
					&& elemType.implOverride.getClass() == NativeTypeConverter.class
					&& ((NativeTypeConverter)elemType.implOverride).specifiedStorage == ValRegister.StoType.BOOL)
					impl= new ArrayOfBoolImpl((TupleType)type, this, encoding);
			}
			if (impl == IMPL_IN_PROGRESS)
				impl= new PackedTupleImpl((TupleType)type, this, encoding);
			break;
		default:
			throw new Error("Unimplemented");
		}
	}
}

class RecordCodec extends TupleCodec {
	TupleCoding encoding;
	Codec[] elemTypes;
	int[] tempElemTypeCodes;

	public boolean equals(Object other) {
		return super.equals(other)
			&& encoding == ((RecordCodec)other).encoding;
	}

	public RecordCodec(RecordType t, TupleCoding encoding) {
		type= t;
		implOverride= type.getCustomCodec();
		this.encoding= encoding;
		elemTypes= new Codec[t.getElemCount()];
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(CodecBuilder cb) {
		for (int i=0; i<elemTypes.length; i++)
			elemTypes[i]= cb.getIncompleteCodecFor(((RecordType)type).getElemType(i));
		initStatus= ISTAT_NEED_IMPL;
	}

	public RecordCodec(Symbol name, UserpReader reader) throws IOException {
		reader.inspectTuple();
		encoding= TupleCoding.values()[reader.readAsInt()];
		int fieldCount= reader.inspectTuple();

		Symbol[] names= new Symbol[fieldCount];
		tempElemTypeCodes= new int[fieldCount];
		for (int i=0; i<fieldCount; i++) {
			reader.inspectTuple();
			names[i]= (Symbol) reader.readValue();
			tempElemTypeCodes[i]= reader.readAsInt();
			reader.closeTuple();
		}
		reader.closeTuple();
		reader.closeTuple();

		type= new RecordType(name)
			.init(new RecordType.RecordDef(names, new UserpType[fieldCount]));
		elemTypes= new Codec[fieldCount];
		initStatus= ISTAT_NEED_DESC_REFS;
	}

	void resolveDescriptorRefs(UserpDecoder.TypeCodeMap tcm) throws UserpProtocolException {
		UserpType[] types= ((RecordType)type).def.fieldTypes;
		for (int i=0; i<elemTypes.length; i++) {
			elemTypes[i]= tcm.getCodec(tempElemTypeCodes[i]);
			types[i]= elemTypes[i].type;
		}
		tempElemTypeCodes= null;
		initStatus= ISTAT_NEED_IMPL;
	}

	protected void serializeFields(UserpWriter writer) throws IOException {
		writer.select(TRecordSpec);
		writer.beginTuple();
		writer.write(encoding.ordinal());
		Symbol[] names= ((RecordType)type).def.fieldNames;
		writer.beginTuple(names.length);
		for (int i=0; i<names.length; i++) {
			writer.beginTuple();
			writer.write(names[i]);
			writer.write(elemTypes[i].encoderTypeCode);
			writer.endTuple();
		}
		writer.endTuple();
		writer.endTuple();
	}

	protected void initCodec() {
		switch (encoding) {
		case PACK:
		case BITPACK:
			impl= new PackedTupleImpl((TupleType) type, this, encoding);
			break;
		default:
			throw new Error("Unimplemented");
		}
	}

	protected void initScalarRange() {
		if (encoding == TupleCoding.INDEX || encoding == TupleCoding.INDEFINITE)
			scalarRange= UserpType.INF;
		else
			scalarRange= elemTypes[0].getScalarRange();
	}
}
