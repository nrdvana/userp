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
	UserCodec userCodec;
	int encoderTypeCode= -1;
	BigInteger scalarRange;

	static Codec deserialize(UserpReader src, Map resolveParams) throws IOException {
		Codec result;
		assert(src.inspectTuple() == 2);
		Symbol name= (Symbol) src.readValue();
		switch (src.inspectUnion()) {
		case 0: result= IntegerCodec.deserialize_IntType(name, src, resolveParams); break;
		case 1: result= UnionCodec.deserialize_UnionType(name, src, resolveParams); break;
		case 2: result= ArrayCodec.deserialize_ArrayType(name, src, resolveParams); break;
		case 3: result= RecordCodec.deserialize_RecordType(name, src, resolveParams); break;
		case 4: result= ScalarCodec.deserialize_EnumType(name, src, resolveParams); break;
		default:
			throw new Error("Bug");
		}
		src.closeTuple();
		return result;
	}

	protected Codec(BigInteger scalarRange) {
		this.scalarRange= scalarRange;
	}

	final void serialize(UserpWriter dest) throws IOException {
		dest.beginTuple();
		dest.write(getType().getName());
		internal_serialize(dest);
		dest.endTuple();
	}
	protected abstract void internal_serialize(UserpWriter dest) throws IOException;

	abstract void resolveRefs(UserpDecoder.TypeMap codeMap, Object params);
	abstract void resolveRefs(CodecCollection codecs);

	abstract UserpType getType();

	BigInteger getScalarRange() {
		return scalarRange;
	}

	abstract void writeValue(UserpWriter writer, Object val) throws IOException;
	void writeValue(UserpWriter writer, long val) throws IOException {
		throw new RuntimeException("Cannot convert java-type 'long' to a value of type "+getType());
	}
	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		throw new RuntimeException("Operation SelectType not applicable to type "+getType());
	}
	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		throw new RuntimeException("Operation SelectType not applicable to type "+getType());
	}
	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		throw new RuntimeException("Operation BeginTuple not applicable to type "+getType());
	}

	abstract int readValue(UserpReader reader) throws IOException;
	abstract void skipValue(UserpReader reader) throws IOException;
	int inspectUnion(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectUnion not applicable to type "+getType());
	}
	int inspectTuple(UserpReader reader) throws IOException {
		throw new RuntimeException("Operation InspectTuple not applicable to type "+getType());
	}

	static final RecordCodec
		CTypeSpec;
	static final UserpType
		TEnumSpec, TIntSpec, TUnionSpec, TArraySpec, TRecordSpec;
	static {
		TIntSpec= Userp.TNull.cloneAs("TIntSpec");
		TEnumSpec= Userp.TWhole.cloneAs("TEnumSpec");
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
			.init(new UserpType[] {TIntSpec, TUnionSpec, TArraySpec, TRecordSpec, TEnumSpec})
			.setEncParams(false, new boolean[] {true, true, true, true, true});
		UserpType TTypeSpec= new RecordType("TTypeSpec").init(new Field[] {
			new Field("Name", Userp.TSymbol),
			new Field("Spec", TSpecUnion)
		});

		CTypeSpec= (RecordCodec) new CodecCollection().getCodecFor(TTypeSpec);
	}
}

class CodecCollection {
	ThreadLocal<Boolean> recursionInProgress= new ThreadLocal<Boolean>();

	protected HashMap<UserpType.TypeHandle,Codec> typeMap;
	protected HashSet<Codec> newCodecs;

	public CodecCollection() {
		typeMap= new HashMap<UserpType.TypeHandle,Codec>();
		newCodecs= new HashSet<Codec>();
	}

	public Codec getCodecFor(UserpType t) {
		return addType(t);
	}

	public Codec addType(UserpType t) {
		boolean recursionRootedHere= false;
		if (recursionInProgress.get() == null) {
			recursionInProgress.set(Boolean.TRUE);
			recursionRootedHere= true;
		}
		Codec result= typeMap.get(t.handle);
		if (result == null) {
			if (typeMap.containsKey(t.handle))
				throw new RuntimeException("Bad circular dependancy: "+t+" must be turned into a codec before it can be turned into a codec");
			typeMap.put(t.handle, null);
			result= codecForType(t, this);
			typeMap.put(t.handle, result);
			newCodecs.add(result);
		}
		if (recursionRootedHere) {
			recursionInProgress.set(null);
			finishCodecInit();
		}
		return result;
	}

	public HashMap<UserpType.TypeHandle,Codec> getCodecMapping() {
		finishCodecInit();
		return typeMap;
	}

	public Collection<Codec> getTypeTableFor(UserpType[] types) {
		throw new Error("Unimplemented");
	}

	protected void finishCodecInit() {
		// can't use an iterator because more codecs could get added to the set
		while (newCodecs.size() > 0) {
			Iterator<Codec> it= newCodecs.iterator();
			Codec incompleteCodec= it.next();
			it.remove();
			incompleteCodec.resolveRefs(this);
		}
	}

	protected static Codec codecForType(UserpType t, CodecCollection codecs) {
		if (t instanceof ScalarType)
			return ScalarCodec.forType((ScalarType)t, codecs);
		else if (t instanceof TupleType)
			return TupleCodec.forType((TupleType)t, codecs);
		else
			return UnionCodec.forType((UnionType)t, codecs);
	}
}

abstract class ScalarCodec extends Codec {
	ScalarType type;

	static Codec forType(ScalarType t, CodecCollection codecs) {
		BigInteger scalarRange= t.getValueCount();
		if (scalarRange == ScalarType.INF || scalarRange.bitLength() >= 64)
			return t.isDoublyInfinite()? new IntegerCodec(t) : new EnumCodec_inf(t);
		else
			return new EnumCodec_63(t);
	}

	static ScalarCodec deserialize_EnumType(Symbol name, UserpReader src, Map resolveParams) throws IOException {
		BigInteger range= (BigInteger) src.readValue();
		EnumType t= new EnumType(name);
		if (range.equals(BigInteger.ZERO) || range.bitLength() >= 64)
			return new EnumCodec_inf(t);
		else
			return new EnumCodec_63(t);
	}

	protected ScalarCodec(ScalarType t) {
		super(t.getValueCount());
		this.type= t;
	}

	void resolveRefs(UserpDecoder.TypeMap codeMap, Object info) {}
	void resolveRefs(CodecCollection codecs) {}

	protected void internal_serialize(UserpWriter dest) throws IOException {
		BigInteger range= scalarRange == UserpType.INF? BigInteger.ZERO : scalarRange;
		dest.select(TEnumSpec);
		dest.write(range);
	}

	public UserpType getType() {
		return type;
	}
}

class IntegerCodec extends ScalarCodec {
	static ScalarCodec deserialize_IntType(Symbol name, UserpReader src, Map resolveParams) throws IOException {
		src.skipValue(); // no actual value, but we have to advance the stream's current-node
		return new IntegerCodec(new IntegerType(name));
	}

	protected IntegerCodec(ScalarType t) {
		super(t);
	}

	protected void internal_serialize(UserpWriter dest) throws IOException {
		dest.select(TIntSpec);
		dest.write(null);
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		throw new Error("Unimplemented");
	}
	void writeValue(UserpWriter writer, long val) throws IOException {
		throw new Error("Unimplemented");
	}
	int readValue(UserpReader reader) throws IOException {
		throw new Error("Unimplemented");
	}
	void skipValue(UserpReader reader) throws IOException {
		throw new Error("Unimplemented");
	}
}

class EnumCodec_inf extends ScalarCodec {
	int bitLen;
	protected EnumCodec_inf(ScalarType t) {
		super(t);
		bitLen= scalarRange == UserpType.INF? -1 : scalarRange.subtract(BigInteger.ONE).bitLength();
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof BigInteger) {
			BigInteger val_bi= (BigInteger) val;
			if (bitLen != -1 && val_bi.compareTo(scalarRange) >= 0)
				throw new IllegalArgumentException("Scalar value out of bounds: "+val+" >= "+scalarRange);
			writer.dest.writeQty(val_bi, bitLen);
		}
		else
			writeValue(writer, ((Number)val).longValue());
	}
	void writeValue(UserpWriter writer, long val) throws IOException {
		// no range check, because we determined that the value could be greater than 63 bits
		writer.dest.writeQty(val, bitLen);
	}
	int readValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
		if (reader.src.scalar != null) {
			if (bitLen != -1 && reader.src.scalar.compareTo(scalarRange) >= 0)
				throw new UserpProtocolException("Scalar value out of bounds: "+reader.src.scalar+" >= "+scalarRange);
			reader.valRegister_o= reader.src.scalar;
			return reader.NATIVETYPE_OBJECT;
		}
		else {
			// no range check, because we determined that the value could be greater than 63 bits
			reader.valRegister_64= reader.src.scalar64;
			return reader.getTypeForBitLen(Util.getBitLength(reader.valRegister_64));
		}
	}
	void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

class EnumCodec_63 extends EnumCodec_inf {
	int stoClass;
	protected EnumCodec_63(ScalarType t) {
		super(t);
		stoClass= UserpReader.getTypeForBitLen(t.getValueCount().subtract(BigInteger.ONE).bitLength());
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		if (val instanceof BigInteger) {
			BigInteger val_bi= (BigInteger) val;
			if (val_bi.compareTo(scalarRange) >= 0)
				throw new IllegalArgumentException("Scalar value out of bounds: "+val+" >= "+scalarRange);
			writer.dest.writeQty(val_bi.longValue(), bitLen);
		}
		else
			writeValue(writer, ((Number)val).longValue());
	}

	void writeValue(UserpWriter writer, long val) throws IOException {
		if (val >= scalarRange.longValue())
			throw new IllegalArgumentException("Scalar value out of bounds: "+val+" >= "+scalarRange);
		writer.dest.writeQty(val, bitLen);
	}

	int readValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
		long val= reader.src.scalarAsLong();
		if (val >= scalarRange.longValue())
			throw new UserpProtocolException("Scalar value out of bounds: "+val+" >= "+scalarRange);
		reader.valRegister_64= val;
		return stoClass;
	}

	void skipValue(UserpReader reader) throws IOException {
		reader.src.readQty(bitLen);
	}
}

class TypeAnyCodec extends UnionCodec {
	TypeAnyCodec() {
		super(null, 0, false, new boolean[0], BigInteger.ZERO);
	}

	protected void internal_serialize(UserpWriter dest) throws IOException {
		throw new UnsupportedOperationException("TypeAnyCodec cannot be serialized");
	}

	void resolveRefs(UserpDecoder.TypeMap codeMap, Object info) {}

	void resolveRefs(CodecCollection codecs) {}

	public UserpType getType() {
		return Userp.TAny;
	}

	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		writer.dest.writeType(whichUnionMember);
		writer.nodeCodec= whichUnionMember;
	}

	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		Codec memberCodec= writer.dest.typeMap.get(whichUnionMember.handle);
		if (memberCodec == null) {
			switch (writer.eventActions[Event.UNDEFINED_TYPE_USED.ordinal()]) {
			case ERROR: throw new RuntimeException("Cannot encode "+whichUnionMember+" because this type has not explicitly been added to this stream");
			case WARN: writer.addWarning("The type "+whichUnionMember+" was implicitly added to the stream");
			case IGNORE:
			}
			memberCodec= writer.addType(whichUnionMember);
		}
		selectType(writer, memberCodec);
	}

	int inspectUnion(UserpReader reader) throws IOException {
		reader.nodeCodec= reader.src.readType();
		return -1;
	}
}

abstract class UnionCodec extends Codec {
	UnionType type;
	Codec[] members;
	int selectorBitLen;
	boolean bitpack;
	boolean[] memberInlineFlags;

	/** Create a codec for UserpType t, with the specified codec params.
	 * This function creates a codec implementation that efficiently encodes the
	 * given type with its given parameters.  The new codec is not complete
	 * until resolveRefs is called, because (due to circular references) some
	 * codec objects might not yet be available.
	 *
	 * @param t UnionType The type to create a codec from
	 * @param bitpack boolean Codec param: Whether to begin bitpacking before writing the selector
	 * @param inlineFlags boolean[] Codec param: Whether to "inline" each member's scalar component
	 * @param typeToDecoder Map A map from type objects to codec objects
	 * @return Codec The newly created codec with unresolved references
	 */
	static Codec forType(UnionType t, boolean bitpack, boolean[] inlineFlags, CodecCollection codecs) {
		BigInteger[] offsets= new BigInteger[t.def.members.length];
		BigInteger range;
		range= calcRangeAndOffsets(t.def.members, inlineFlags, offsets, codecs);
		return (range.bitLength() < 64)?
			new UnionCodec_63(t, bitpack, inlineFlags, range, offsets)
			: new UnionCodec_bi(t, bitpack, inlineFlags, range, offsets);
	}

	/** Create a codec for UserpType t with the default codec params.
	 * This routine is simply a convenience method for "forType" that gets the
	 * missing codec parameters from the type's encoding hints, or uses default
	 * values if the hints were not set.
	 *
	 * @param t UnionType The type to create a codec from
	 * @param typeToDecoder Map A map from type objects to codec objects
	 * @return Codec The newly created codec with unresolved references
	 */
	static Codec forType(UnionType t, CodecCollection codecs) {
		boolean[] inlineFlags= t.getEncoderParam_Inline();
		if (inlineFlags == null) {
			inlineFlags= new boolean[t.getMemberCount()];
			Arrays.fill(inlineFlags, false);
		}
		return forType(t, t.getEncoderParam_Bitpack(), inlineFlags, codecs);
	}

	/** Deserialize a codec from a Userp type table.
	 * This function first deserializes a codec definition form the supplied
	 * reader, then creates a codec instance from it.  The codec object will
	 * not be usable until resolveRefs is called, because some codec objects
	 * might not have been created yet.
	 *
	 * @param name Symbol The name of the type that this codec implements
	 * @param src UserpReader A reader from which the definition will be read
	 * @param resolveParams Map A map from a Codec to the parameter it needs during 'resolve'
	 * @return UnionCodec A new codec object with unresolved references
	 * @throws IOException On any read error, or if invalid references occour.
	 */
	static UnionCodec deserialize_UnionType(Symbol name, UserpReader src, Map resolveParams) throws IOException {
		src.inspectTuple();
		boolean bitpack= src.readAsBool();
		int memberCount= src.inspectTuple();
		int[] typeCodes= new int[memberCount];
		for (int i=0; i<memberCount; i++)
			typeCodes[i]= src.readAsInt();
		src.closeTuple();
		boolean[] inlineFlags= (boolean[]) src.readValue();
		src.closeTuple();

		Codec[] codecs= new Codec[memberCount];
		for (int i=0; i<memberCount; i++) {
			codecs[i]= src.src.codeMap.getType(typeCodes[i]);
			if (inlineFlags[i] && codecs[i] == null)
				throw new UserpProtocolException("Illegal dependancy, or improper encoding order");
		}
		UnionType t= new UnionType(name);
		BigInteger[] offsets= new BigInteger[typeCodes.length];
		BigInteger range= calcRangeAndOffsets(codecs, inlineFlags, offsets, null);
		UnionCodec result= (range.bitLength() < 64)?
			new UnionCodec_63(t, bitpack, inlineFlags, range, offsets)
			: new UnionCodec_bi(t, bitpack, inlineFlags, range, offsets);
		resolveParams.put(result, typeCodes);
		return result;
	}

	protected UnionCodec(UnionType t, int memberCount, boolean bitpack, boolean[] inlineFlags, BigInteger scalarRange) {
		super(scalarRange);
		this.type= t;
		this.members= new Codec[memberCount];
		this.bitpack= bitpack;
		this.selectorBitLen= scalarRange == UserpType.INF? -1 : scalarRange.subtract(BigInteger.ONE).bitLength();
		this.memberInlineFlags= inlineFlags;
		if (inlineFlags.length != memberCount)
			throw new IllegalArgumentException("length of 'inlineFlags' array must match memberCount");
	}

	/** Resolve references to codec objects.
	 * This function finishes the construction of the Codec object started by a
	 * call to deserialize_UnionType.  After this call, the Codec object is
	 * ready for use.
	 *
	 * @param codeMap TypeMap The same typecode mapping that was supplied to 'deserialize'
	 * @param params Object The parameter that 'deserialize' mapped to this object.
	 */
	void resolveRefs(UserpDecoder.TypeMap codeMap, Object params) {
		int[] typeCodes= (int[]) params;
		UserpType[] memberTypes= new UserpType[members.length];
		for (int i=0; i<members.length; i++) {
			members[i]= codeMap.getType(typeCodes[i]);
			memberTypes[i]= members[i].getType();
		}
		type.init(memberTypes);
	}

	/** Resolve references to codec objects.
	 * This function finishes the construction of the Codec object started by a
	 * call to 'forType'.  After this call, the Codec object is ready for use.
	 *
	 * @param typeMap Map A map from types to their codecs
	 */
	void resolveRefs(CodecCollection codecs) {
		for (int i=0; i<members.length; i++)
			members[i]= codecs.getCodecFor(type.getMember(i));
	}

	protected void internal_serialize(UserpWriter dest) throws IOException {
		dest.select(TUnionSpec);
		dest.beginTuple();
		dest.write(bitpack?1:0);
		dest.beginTuple(type.getMemberCount());
		for (int i=0; i<members.length; i++) {
			dest.write(members[i].encoderTypeCode);
			assert(!memberInlineFlags[i] || members[i].encoderTypeCode < encoderTypeCode);
		}
		dest.endTuple();
		dest.write(memberInlineFlags);
		dest.endTuple();
	}

	public UserpType getType() {
		return type;
	}

	void writeValue(UserpWriter writer, Object val) throws IOException {
		TypedData td= (TypedData) val;
		selectType(writer, td.dataType);
		writer.write(td.data);
	}

	int readValue(UserpReader reader) throws IOException {
		inspectUnion(reader);
		UserpType t= reader.getType();
		reader.valRegister_o= new TypedData(t, reader.readValue());
		return reader.NATIVETYPE_OBJECT;
	}

	void skipValue(UserpReader reader) throws IOException {
		inspectUnion(reader);
		reader.skipValue();
	}

	private static BigInteger calcRangeAndOffsets(Object[] members, boolean[] inlineFlags, BigInteger[] offsetsResult, CodecCollection codecs) {
		BigInteger curOffset= BigInteger.ZERO;
		BigInteger[] ofs= offsetsResult; // rename for convenience
		boolean[] inlined= inlineFlags;
		for (int i= 0; i<members.length; i++) {
			ofs[i]= curOffset;
			BigInteger curRange;
			if (!inlined[i])
				curRange= BigInteger.ONE;
			else { // else we need to know how big a scalar range to assign to this member
				if (members[i] instanceof Codec)
					// All codecs know their scalar range
					curRange= ((Codec)members[i]).getScalarRange();
				else if (members[i] instanceof ScalarType)
					// ScalarType knows its range from just the type
					curRange= ((ScalarType)members[i]).getValueCount();
				else {
					// for all other userpType objects, we need a codec
					Codec c= codecs.getCodecFor((UserpType)members[i]);
					if (c == null) // no codec? Illegal Dependancy!
						throw new RuntimeException("Illegal type dependancy: directed to inline "+members[i]+", but it's encoding parameters are not yet known");
					curRange= c.getScalarRange();
					if (curRange == null)
						throw new RuntimeException("sub-type "+members[i]+" has no scalar component, but was specified to be inlined");
				}
			}
			if (curRange == UserpType.INF) {
				if (i != members.length-1)
					throw new RuntimeException("sub-type "+members[i]+" is infinite, and inlined, and is not the last sub-type");
				curOffset= UserpType.INF;
			}
			else
				curOffset= curOffset.add(curRange);
		}
		return curOffset;
	}
}

class UnionCodec_bi extends UnionCodec {
	BigInteger[] offsets;

	public UnionCodec_bi(UnionType t, boolean bitpack, boolean[] inlineFlags, BigInteger scalarRange, BigInteger[] offsets) {
		super(t, offsets.length, bitpack, inlineFlags, scalarRange);
		this.offsets= offsets;
	}

	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		selectType(writer, whichUnionMember.getType());
	}

	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		if (bitpack)
			writer.dest.enableBitpack(true);
		int memberIdx= type.def.getMemberIdx(whichUnionMember);
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

class UnionCodec_63 extends UnionCodec {
	long[] offsets;

	public UnionCodec_63(UnionType t, boolean bitpack, boolean[] inlineFlags, BigInteger scalarRange, BigInteger[] offsets) {
		super(t, offsets.length, bitpack, inlineFlags, scalarRange);
		this.offsets= new long[offsets.length];
		for (int i=0; i<offsets.length; i++)
			this.offsets[i]= offsets[i].longValue();
	}

	void selectType(UserpWriter writer, Codec whichUnionMember) throws IOException {
		selectType(writer, whichUnionMember.getType());
	}

	void selectType(UserpWriter writer, UserpType whichUnionMember) throws IOException {
		if (bitpack)
			writer.dest.enableBitpack(true);
		int memberIdx= type.def.getMemberIdx(whichUnionMember);
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

abstract class TupleCodec extends Codec {
	TupleType type;
	TupleCoding encoding;

	static Codec forType(TupleType t, CodecCollection codecs) {
		if (t instanceof ArrayType)
			return ArrayCodec.forType((ArrayType)t, codecs);
		else
			return RecordCodec.forType((RecordType)t, codecs);
	}

	protected TupleCodec(BigInteger scalarRange, TupleType t, TupleCoding encoding) {
		super(scalarRange);
		this.type= t;
		this.encoding= encoding;
	}

	static final BigInteger calcScalarRange(TupleCoding encoding, boolean fixedElemCount, TupleType tupleType, CodecCollection codecs) {
		BigInteger result= getRangeOfSpecialScalar(encoding, fixedElemCount);
		if (result == null) {
			if (tupleType.getDefinition() == null || tupleType.getElemType(0) == null)
				throw new RuntimeException("Type "+tupleType.getName()+" needs to be initialized before it can be converted to a codec");
			result= codecs.getCodecFor(tupleType.getElemType(0)).getScalarRange();
		}
		return result;
	}

	static final BigInteger calcScalarRange(TupleCoding encoding, boolean fixedElemCount, int firstElemTypeID, UserpDecoder.TypeMap typeMap) {
		BigInteger result= getRangeOfSpecialScalar(encoding, fixedElemCount);
		if (result == null) {
			Codec elemCodec= typeMap.getType(firstElemTypeID);
			if (elemCodec == null)
				throw new RuntimeException("Circular dependandy in type table");
			result= elemCodec.getScalarRange();
		}
		return result;
	}
	protected static final BigInteger getRangeOfSpecialScalar(TupleCoding encoding, boolean fixedElemCount) {
		switch (encoding) {
		case PACK:
		case BITPACK:    return fixedElemCount? null : UserpType.INF;
		case INDEX:      return UserpType.INF;
		case INDEFINITE: return UserpType.INF;
		default:
			throw new Error();
		}
	}

	public UserpType getType() {
		return type;
	}

	abstract Codec elemCodec(int idx);

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

	void beginTuple(UserpWriter writer, int elemCount) throws IOException {
		switch (encoding) {
		case BITPACK:
		case PACK:
			packed_beginTuple(writer, elemCount); break;
		default:
			throw new Error("Unimplemented");
		}
	}

	void nextElement(UserpWriter writer) throws IOException {
		switch (encoding) {
		case PACK:
		case BITPACK:
			packed_nextElement(writer); break;
		default:
			throw new Error("Unimplemented");
		}
	}

	void endTuple(UserpWriter writer) throws IOException {
		switch (encoding) {
		case PACK:
		case BITPACK:
			packed_endTuple(writer); break;
		default:
			throw new Error("Unimplemented");
		}
	}

	int readValue(UserpReader reader) throws IOException {
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
			reader.valRegister_o= result.toArray();
		}
		else {
			Object[] result= new Object[elemCount];
			for (int i=0; i<elemCount; i++)
				result[i]= reader.readValue();
			reader.valRegister_o= result;
		}
		reader.closeTuple();
		return reader.NATIVETYPE_OBJECT;
	}

	void skipValue(UserpReader reader) throws IOException {
		reader.inspectTuple();
		while (reader.getType() != null)
			reader.skipValue();
		reader.closeTuple();
	}

	int inspectTuple(UserpReader reader) throws IOException {
		switch (encoding) {
		case PACK:
		case BITPACK:
			return packed_inspectTuple(reader);
		default:
			throw new Error("Unimplemented");
		}
	}

	void nextElement(UserpReader writer) throws IOException {
		switch (encoding) {
		case PACK:
		case BITPACK:
			packed_nextElement(writer); break;
		default:
			throw new Error("Unimplemented");
		}
	}

	void closeTuple(UserpReader reader) throws IOException {
		switch (encoding) {
		case PACK:
		case BITPACK:
			packed_closeTuple(reader); break;
		default:
			throw new Error("Unimplemented");
		}
	}

	int packed_inspectTuple(UserpReader reader) throws IOException {
		reader.tupleState.tupleCodec= this;
		reader.tupleState.elemCount= readElemCount(reader);
		reader.tupleState.elemIdx= 0;
		reader.nodeCodec= elemCodec(0);
		if (encoding == TupleCoding.BITPACK)
			reader.src.enableBitpack(true);
		reader.tupleState.bitpack= reader.src.bitpack;
		return reader.tupleState.elemCount;
	}

	void packed_nextElement(UserpReader reader) throws IOException {
		reader.nodeCodec= (++reader.tupleState.elemIdx >= reader.tupleState.elemCount)?
			TupleEndSentinelCodec.INSTANCE : elemCodec(reader.tupleState.elemIdx);
		reader.src.enableBitpack(reader.tupleState.bitpack);
	}

	void packed_closeTuple(UserpReader reader) throws IOException {
		while (reader.tupleState.elemIdx < reader.tupleState.elemCount)
			reader.skipValue();
		assert(reader.nodeCodec == TupleEndSentinelCodec.INSTANCE);
	}

	void packed_beginTuple(UserpWriter writer, int elemCount) throws IOException {
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
		writer.tupleState.elemIdx= -1;
		if (encoding == TupleCoding.BITPACK)
			writer.dest.enableBitpack(true);
		writer.tupleState.bitpack= writer.dest.bitpack;
	}

	void packed_nextElement(UserpWriter writer) throws IOException {
		writer.nodeCodec= (++writer.tupleState.elemIdx >= writer.tupleState.elemCount)?
			TupleEndSentinelCodec.INSTANCE : elemCodec(writer.tupleState.elemIdx);
		writer.dest.enableBitpack(writer.tupleState.bitpack);
	}

	void packed_endTuple(UserpWriter writer) throws IOException {
		if (writer.tupleState.elemIdx != writer.tupleState.elemCount-1)
			throw new RuntimeException(type.getDisplayName()+" has "+writer.tupleState.elemCount+" elements, but only "+writer.tupleState.elemIdx+" were written.");
	}

	int readElemCount(UserpReader reader) throws IOException {
		int count= type.getElemCount();
		if (count < 0) {
			reader.src.readVarQty();
			count= reader.src.scalarAsInt();
		}
		return count;
	}
}

class ArrayCodec extends TupleCodec {
	Codec elemCodec;
	int length;

	static Codec forType(ArrayType t, CodecCollection codecs) {
		TupleCoding defEnc= t.getEncoderParam_TupleCoding();
		return forType(t, defEnc == null? TupleCoding.PACK : defEnc, codecs);
	}
	static Codec forType(ArrayType t, TupleCoding encoding, CodecCollection codecs) {
		return new ArrayCodec(t, encoding, calcScalarRange(encoding, t.getElemCount() != -1, t, codecs));
	}

	static TupleCodec deserialize_ArrayType(Symbol name, UserpReader src, Map resolveParams) throws IOException {
		src.inspectTuple();
		TupleCoding encoding= TupleCoding.values()[src.readAsInt()];
		int elemTypeCode= src.readAsInt();
		int length= src.readAsInt()-1; // value 0 is VarLen, now becomes -1
		src.closeTuple();

		ArrayType t= new ArrayType(name).init(null, length);
		ArrayCodec result= new ArrayCodec(t, encoding, calcScalarRange(encoding, length != -1, elemTypeCode, src.src.codeMap));
		resolveParams.put(result, new Integer(elemTypeCode));
		return result;
	}

	void resolveRefs(UserpDecoder.TypeMap codeMap, Object params) {
		int elemTypeCode= (Integer) params;
		elemCodec= codeMap.getType(elemTypeCode);
		((ArrayType)type).def.elemType= elemCodec.getType();
	}
	void resolveRefs(CodecCollection codecs) {
		elemCodec= codecs.getCodecFor(type.getElemType(0));
	}

	protected ArrayCodec(ArrayType t, TupleCoding encoding, BigInteger scalarRange) {
		super(scalarRange, t, encoding);
		this.length= t.getElemCount();
	}

	Codec elemCodec(int idx) {
		return elemCodec;
	}

/*	Class elemClass= arrayDef.typeRefs[0].preferredNativeStorage;
	if (elemClass != null)
		preferredNativeStorage= java.lang.reflect.Array.newInstance(elemClass, 0).getClass();*/
	protected void internal_serialize(UserpWriter dest) throws IOException {
		dest.select(TArraySpec);
		dest.beginTuple();
		dest.write(encoding.ordinal());
		dest.write(elemCodec.encoderTypeCode);
		dest.write(length+1); // INF == -1, so the scalar is 0=INF,[1..N]=[0..N-1]
		dest.endTuple();
	}
}

class RecordCodec extends TupleCodec {
	Symbol[] names;
	Codec[] elemCodecs;

	static Codec forType(RecordType t, CodecCollection codecs) {
		TupleCoding defEnc= t.getEncoderParam_TupleCoding();
		return forType(t, defEnc == null? TupleCoding.PACK : defEnc, codecs);
	}
	static Codec forType(RecordType t, TupleCoding encoding, CodecCollection codecs) {
		return new RecordCodec(t, encoding, t.def.fieldNames, calcScalarRange(encoding, true, t, codecs));
	}

	static RecordCodec deserialize_RecordType(Symbol name, UserpReader src, Map resolveParams) throws IOException {
		src.inspectTuple();
		TupleCoding encoding= TupleCoding.values()[src.readAsInt()];
		int fieldCount= src.inspectTuple();
		Symbol[] names= new Symbol[fieldCount];
		int[] typeCodes= new int[fieldCount];
		for (int i=0; i<fieldCount; i++) {
			src.inspectTuple();
			names[i]= (Symbol) src.readValue();
			typeCodes[i]= src.readAsInt();
			src.closeTuple();
		}
		src.closeTuple();
		src.closeTuple();

		RecordType t= new RecordType(name);
		RecordCodec result= new RecordCodec(t, encoding, names, calcScalarRange(encoding, true, typeCodes[0], src.src.codeMap));
		resolveParams.put(result, typeCodes);
		return result;
	}

	protected RecordCodec(RecordType t, TupleCoding encoding, Symbol[] names, BigInteger scalarRange) {
		super(scalarRange, t, encoding);
		this.names= names;
		this.elemCodecs= new Codec[names.length];
	}

	void resolveRefs(UserpDecoder.TypeMap codeMap, Object info) {
		int[] types= (int[]) info;
		for (int i=0; i<elemCodecs.length; i++)
			elemCodecs[i]= codeMap.getType(types[i]);
	}

	void resolveRefs(CodecCollection codecs) {
		for (int i=0; i<elemCodecs.length; i++)
			elemCodecs[i]= codecs.getCodecFor(type.getElemType(i));
	}

	protected void internal_serialize(UserpWriter dest) throws IOException {
		dest.select(TRecordSpec);
		dest.beginTuple();
		dest.write(encoding.ordinal());
		dest.beginTuple(names.length);
		for (int i=0; i<names.length; i++) {
			dest.beginTuple();
			dest.write(names[i]);
			dest.write(elemCodecs[i].encoderTypeCode);
			dest.endTuple();
		}
		dest.endTuple();
		dest.endTuple();
	}

	Codec elemCodec(int idx) {
		return elemCodecs[idx];
	}
}
