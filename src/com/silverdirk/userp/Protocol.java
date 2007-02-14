package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Protocol definitions</p>
 * <p>Description: This class contains all the types used in the protocol</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class Protocol {
	static final UserpType[] VERSION_0_TYPES= new UserpType[] {
		FundamentalTypes.TAny,
		FundamentalTypes.TType,
		FundamentalTypes.TWhole,
		FundamentalTypes.TNegWhole,
		CommonTypes.TNull,
		CommonTypes.TBool,
		CommonTypes.TInt,
		CommonTypes.TInt8,
		CommonTypes.TInt16,
		CommonTypes.TInt32,
		CommonTypes.TInt64,
		CommonTypes.TByte,
		CommonTypes.TInt16u,
		CommonTypes.TInt32u,
		CommonTypes.TInt64u,
		CommonTypes.TFloat,
		CommonTypes.TFloat32,
		CommonTypes.TFloat64,
		CommonTypes.TStrUTF8,
		CommonTypes.TStrUTF16,
		CommonTypes.TTime_ms,
		CommonTypes.TTime_s_frac,
		CommonTypes.TTimeLocal_ms,
		CommonTypes.TTimeLocal_s_frac,
		CommonTypes.TList,
		CommonTypes.TIndexedList,
		CommonTypes.TIndefiniteList,
		CommonTypes.TSymbol,
		CommonTypes.TSymbolArray,
		CommonTypes.TBoolArray,
		CommonTypes.TByteArray,
		ProtocolTypes.TTypeName
	};
}

class TypeNameSnafuTypes {
	public static final RangeType TByte;
	public static final ArrayType TSymbol, TTypeName;
	public static final WholeType TWhole;
	static {
		Object[] empty= new Object[0];
		TWhole= new WholeType(empty, WholeType.POSITIVE);
		TByte= new RangeType(empty, TWhole, 0, 255);
		TByte.preferredNativeStorage= byte.class;
		TSymbol= new ArrayType(empty, TByte, TypeDef.VARIABLE_LEN);
		TTypeName= (ArrayType) TSymbol.makeSynonym(empty);
		TTypeName.lateInitMeta(UserpType.nameToMeta("TTypeName"));
		TWhole.lateInitMeta(UserpType.nameToMeta("TWhole"));
		TByte.lateInitMeta(UserpType.nameToMeta("TByte"));
		TSymbol.lateInitMeta(UserpType.nameToMeta("TSymbol"));
		TByte.overrideImpl(UnsignedImpl.INSTANCE_UBYTE);
	}
}

class FundamentalTypes {
	public static final AnyType TAny= AnyType.INSTANCE_TANY;
	public static final TypeType TType= TypeType.INSTANCE_TTYPE;
	public static final WholeType
		TWhole=    TypeNameSnafuTypes.TWhole,
		TNegWhole= new WholeType(UserpType.nameToMeta("TNegWhole"), WholeType.NEGATIVE);
}

class CommonTypes {
	protected static final EnumType
		TSign= new EnumType("FloatSignBit", new String[] {"Positive", "Negative"}),
		TDenormFloat= new EnumType("Denormalized", new String[] {"0/Denormalized"}),
		TNaNFloat= new EnumType("NaN", new String[] {"Inf/NaN"});

	public static final UnionType
		TInt= (UnionType) new UnionType("TInt",
			new UserpType[] {
				FundamentalTypes.TNegWhole,
				new RangeType("1..Inf", FundamentalTypes.TWhole, BigInteger.ONE, TypeDef.INFINITE)
			},
			new boolean[] {false, true}
		).overrideImpl(BigIntImpl.INSTANCE),
		TInt8=   createSignedInt("TInt8",   8, byte.class,  SignedImpl.INSTANCE_BYTE),
		TInt16=  createSignedInt("TInt16", 16, short.class, SignedImpl.INSTANCE_SHORT),
		TInt32=  createSignedInt("TInt32", 32, int.class,   SignedImpl.INSTANCE_INT),
		TInt64=  createSignedInt("TInt64", 64, long.class,  SignedImpl.INSTANCE_LONG),
		TTime_ms=   (UnionType) TInt.makeSynonym("TTime_ms").overrideImpl(DateImpl.INSTANCE),
		TTimeLocal_ms= (UnionType) TInt.makeSynonym("TTimeLocal_ms").overrideImpl(RelativeDateImpl.INSTANCE);
	public static final RangeType
		TByte=   TypeNameSnafuTypes.TByte,
		TInt16u= (RangeType) new RangeType("TInt16u", FundamentalTypes.TWhole, 0, 0xFFFFL).overrideImpl(UnsignedImpl.INSTANCE_USHORT),
		TInt32u= (RangeType) new RangeType("TInt32u", FundamentalTypes.TWhole, 0, 0xFFFFFFFFL).overrideImpl(UnsignedImpl.INSTANCE_UINT),
		TInt64u= (RangeType) new RangeType("TInt64u", FundamentalTypes.TWhole, BigInteger.ZERO, BigInteger.ONE.shiftLeft(64).subtract(BigInteger.ONE)).overrideImpl(UnsignedImpl.INSTANCE_ULONG);
	public static final EnumType
		TBool= (EnumType) new EnumType("TBool", new String[] { "False", "True" }).overrideImpl(BoolImpl.INSTANCE),
		TNull= (EnumType) new EnumType("TNull", new String[0]).overrideImpl(NullImpl.INSTANCE);
	static {
		TInt16u.setNativeStoragePref(short.class);
		TInt32u.setNativeStoragePref(int.class);
		TInt64u.setNativeStoragePref(long.class);
		TInt.setNativeStoragePref(BigInteger.class);
		TTime_ms.setNativeStoragePref(java.util.Date.class);
		TTimeLocal_ms.setNativeStoragePref(java.util.Date.class);
		TBool.setNativeStoragePref(boolean.class);
	}
	public static final RecordType
		TFloat= new RecordType("TFloat", new String[] {"Significand", "Base", "Exponent"}, new UserpType[] { TInt, FundamentalTypes.TWhole, TInt });
	public static final RecordType
		TFloat32= createFloat("TFloat32", false, FloatImpl.INSTANCE_FLOAT),
		TFloat64= createFloat("TFloat64", true,  FloatImpl.INSTANCE_DOUBLE),
		TTime_s_frac= (RecordType) TFloat.makeSynonym("TTime_sfrac"),
		TTimeLocal_s_frac= (RecordType) TFloat.makeSynonym("TTimeLocal_sfrac");
	public static final ArrayType
		TStrUTF8=  (ArrayType) new ArrayType("TUTF8", TByte, TypeDef.VARIABLE_LEN).overrideImpl(StringImpl.INSTANCE_UTF8),
		TStrUTF16= (ArrayType) new ArrayType("TUTF16", TInt16u, TypeDef.VARIABLE_LEN).overrideImpl(StringImpl.INSTANCE_UTF16),
		TList= new ArrayType("TList", ArrayType.TupleCoding.TIGHT, FundamentalTypes.TAny, TypeDef.VARIABLE_LEN),
		TIndexedList= new ArrayType("TIndexedList", ArrayType.TupleCoding.INDEXED, FundamentalTypes.TAny, TypeDef.VARIABLE_LEN),
		TIndefiniteList= new ArrayType("TIndefiniteList", ArrayType.TupleCoding.INDEFINITE, FundamentalTypes.TAny, TypeDef.VARIABLE_LEN),
		TStringArray= new ArrayType("TStringArray", TStrUTF8),
		TSymbol= TypeNameSnafuTypes.TSymbol,
		TSymbolArray= new ArrayType("TSymbolList", TSymbol),
		TBoolArray= new ArrayType("TBoolList", ArrayType.TupleCoding.BITPACK, TBool, TypeDef.VARIABLE_LEN),
		TByteArray= (ArrayType) new ArrayType("TByteArray", ArrayType.TupleCoding.TIGHT, TByte, TypeDef.VARIABLE_LEN).overrideImpl(ByteArrayImpl.INSTANCE);

	static UnionType createSignedInt(String name, int bits, Class nativeType, ReaderWriterImpl impl) {
		RangeType pos= new RangeType("+", FundamentalTypes.TWhole, BigInteger.ZERO, BigInteger.ONE.shiftLeft(bits-1).subtract(BigInteger.ONE));
		RangeType neg= new RangeType("-", FundamentalTypes.TNegWhole, BigInteger.ONE.shiftLeft(bits-1), BigInteger.ONE);
		UnionType result= new UnionType(name, new UserpType[] { pos, neg }, new boolean[] { true, true });
		result.setNativeStoragePref(nativeType);
		result.overrideImpl(impl);
		return result;
	}

	static RecordType createFloat(String name, boolean dbl, ReaderWriterImpl impl) {
		UnionType exponent= new UnionType("FloatExponent",
			new UserpType[] {
				TDenormFloat,
				new RangeType("-", FundamentalTypes.TNegWhole, dbl? 0x3FE : 0x7E, 1),
				new RangeType("+", FundamentalTypes.TWhole, 0, dbl? 0x3FF : 0x7F),
				TNaNFloat
			},
			new boolean[] { true, true, true, true }
		);
		RangeType significand= new RangeType("FloatMantissa", FundamentalTypes.TWhole, 0, dbl? 0xFFFFFFFFFFFFFL : 0x7FFFFF);
		RecordType result= new RecordType(name, RecordType.TupleCoding.BITPACK,
			new String[] {"Sign", "Exponent", "Mantissa"},
			new UserpType[] { TSign, exponent, significand });
		result.setNativeStoragePref(dbl? double.class : float.class);
		result.overrideImpl(impl);
		return result;
	}
}

class ProtocolTypes {
	public static final ArrayType
		TTypeName= TypeNameSnafuTypes.TTypeName,
		TMetadata= new ArrayType("TMetadata", ArrayType.TupleCoding.TIGHT, FundamentalTypes.TAny, TypeDef.VARIABLE_LEN),
		TEnumData= (ArrayType) TMetadata.makeSynonym("TEnumData");
	static final EnumType
		TInfinity= new EnumType("TInfinity", new String[] {"Inf"}),
		TVarLen= new EnumType("TVarLen", new String[] {"VarLen"}),
		TTupleCoding= new EnumType("TTupleCoding", new String[] {"Indefinite", "Indexed", "Tight", "Bitpack"});
	static final UnionType
		TInfOrVal= new UnionType("TInfOrVal",
			new UserpType[] { TInfinity, FundamentalTypes.TWhole },
			new boolean[] { true, true }
		),
		TVarLenOrVal= new UnionType("TVarLenOrVal",
			new UserpType[] { TVarLen, FundamentalTypes.TWhole },
			new boolean[] { true, true }
		);
	static final RecordType
		TUnionMember= new RecordType("TUnionMember",
			RecordType.TupleCoding.TIGHT,
			new String[] { "Type", "Inline" },
			new UserpType[] { FundamentalTypes.TType, CommonTypes.TBool }
		),
		TField= new RecordType("TField",
			new String[] {"Name", "Type"},
			new UserpType[] { CommonTypes.TSymbol, FundamentalTypes.TType }
		);
	static final ArrayType
		TUnionMemberList= new ArrayType("TUnionMemberList",
			ArrayType.TupleCoding.TIGHT, TUnionMember, TypeDef.VARIABLE_LEN
		),
		TFieldList= new ArrayType("TUnionMemberList",
			ArrayType.TupleCoding.TIGHT, TField, TypeDef.VARIABLE_LEN
		);
	static final UserpType
		TSynonymDef= new RecordType("TSynonymDef",
			RecordType.TupleCoding.TIGHT,
			new String[] { "SynOrigin" },
			new UserpType[] { FundamentalTypes.TType }
		),
		TSymEnumDef= CommonTypes.TSymbolArray.makeSynonym("TSymEnumDef"),
		TValEnumDef= CommonTypes.TList.makeSynonym("TValEnumDef"),
		TRangeDef= new RecordType("TRangeDef",
			RecordType.TupleCoding.TIGHT,
			new String[] {"Base", "From", "To"},
			new UserpType[] { FundamentalTypes.TType, FundamentalTypes.TWhole, TInfOrVal }
		),
		TUnionDef= new RecordType("TUnionDef",
			RecordType.TupleCoding.TIGHT,
			new String[] {"Alignment", "Members"},
			new UserpType[] { CommonTypes.TBool, TUnionMemberList }
		),
		TArrayDef= new RecordType("TArrayDef",
			RecordType.TupleCoding.TIGHT,
			new String[] {"TupleCoding", "ElemType", "LegthSpec"},
			new UserpType[] { TTupleCoding, FundamentalTypes.TType, TVarLenOrVal }
		),
		TRecordDef= new RecordType("TRecordDef",
			RecordType.TupleCoding.TIGHT,
			new String[] {"TupleCoding", "Fields"},
			new UserpType[] { TTupleCoding, TFieldList }
		);
	public static final UnionType
		TTypedef= new UnionType("TTypedef",
			new UserpType[] { TSynonymDef, TSymEnumDef, TValEnumDef, TRangeDef, TUnionDef, TArrayDef, TRecordDef },
			new boolean[] { false, false, false, false, true, true, true }
		);
}
