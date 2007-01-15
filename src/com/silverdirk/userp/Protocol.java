package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public class Protocol {
	public Protocol() {
	}
}

class FundamentalTypes {
	public static final AnyType TAny= AnyType.INSTANCE_TANY;
	public static final TypeType TType= TypeType.INSTANCE_TTYPE;
	public static final WholeType
		TWhole=    WholeType.INSTANCE_TWHOLE,
		TNegWhole= WholeType.INSTANCE_TNEGWHOLE;
}

class CommonTypes {
	public static final UnionType
		TInt= new UnionType("TInt",
			new UserpType[] {
				FundamentalTypes.TNegWhole,
				new RangeType("1..Inf", FundamentalTypes.TWhole, BigInteger.ONE, TypeDef.INFINITE)
			},
			new boolean[] {false, true}
		),
		TInt8=   createSignedInt("TInt8",  8 ),
		TInt16=  createSignedInt("TInt16", 16),
		TInt32=  createSignedInt("TInt32", 32),
		TInt64=  createSignedInt("TInt64", 64),
		TTime_ms=   (UnionType) TInt.makeSynonym("TTime_ms"),
		TTimeLocal_ms= (UnionType) TInt.makeSynonym("TTimeLocal_ms");
	public static final RangeType
		TByte=   createUnsignedInt("TByte",   8 ),
		TInt16u= createUnsignedInt("TInt16u", 16),
		TInt32u= createUnsignedInt("TInt32u", 32),
		TInt64u= createUnsignedInt("TInt64u", 64);
	public static final EnumType
		TBool= new EnumType("TBool", true, new String[] { "False", "True" }),
		TNull= new EnumType("TNull", true, new String[0]);
	static {
//		TInt.javaCls= BigInteger.class;
//		TTime_ms.javaCls= java.util.Date.class;
//		TTime_ms.setStorage(JavaDateStorage.INSTANCE);
//		TTimeLocal_ms.setStorage(JavaLocalDateStorage.INSTANCE);
//		TBool.javaCls= boolean.class;
//		TBool.setCoder(OptimizedBooleanImpl.INSTANCE);
//		TBool.setStorage(OptimizedBooleanImpl.INSTANCE);
	}
	public static final RecordType
		TFloat= new RecordType("TFloat", new String[] {"Significand", "Base", "Exponent"}, new UserpType[] { TInt, FundamentalTypes.TWhole, TInt });
	static {
//		TFloat.setStorage(VarPrecisionFloatImpl.INSTANCE);
	}
	public static final RecordType
		TFloat32= createFloat("TFloat32", false),
		TFloat64= createFloat("TFloat64", true),
		TTime_s_frac= (RecordType) TFloat.makeSynonym("TTime_sfrac"),
		TTimeLocal_s_frac= (RecordType) TFloat.makeSynonym("TTimeLocal_sfrac");
	public static final ArrayType
		TStrUTF8=  new ArrayType("TUTF8", TByte, TypeDef.VARIABLE_LEN),
		TStrUTF16= new ArrayType("TUTF8", TInt16u, TypeDef.VARIABLE_LEN),
		TList= new ArrayType("TList", FundamentalTypes.TAny),
		TStringArray= new ArrayType("TStringArray", TStrUTF8),
		TSymbol= (ArrayType) TStrUTF8.makeSynonym("TSymbol"),
		TSymbolArray= new ArrayType("TSymbolList", TSymbol),
		TBoolArray= new ArrayType("TBoolList", ArrayType.TupleCoding.BITPACK, TBool, TypeDef.VARIABLE_LEN),
		TByteArray= new ArrayType("TByteArray", ArrayType.TupleCoding.TIGHT, TByte, TypeDef.VARIABLE_LEN);

	static UnionType createSignedInt(String name, int bits) {
		RangeType pos= new RangeType("+", FundamentalTypes.TWhole, BigInteger.ZERO, BigInteger.ONE.shiftLeft(bits-1).subtract(BigInteger.ONE));
		RangeType neg= new RangeType("-", FundamentalTypes.TNegWhole, BigInteger.ONE.shiftLeft(bits-1), BigInteger.ONE);
		return new UnionType(name, new UserpType[] { pos, neg }, new boolean[] { true, true });
	}

	static RangeType createUnsignedInt(String name, int bits) {
		return new RangeType(name, FundamentalTypes.TWhole, BigInteger.ZERO, BigInteger.ONE.shiftLeft(bits).subtract(BigInteger.ONE));
	}

	protected static final EnumType
		TSign= new EnumType("FloatSignBit", true, new String[] {"Positive", "Negative"}),
		TDenormFloat= new EnumType("Denormalized", true, new String[] {"0/Denormalized"}),
		TNaNFloat= new EnumType("NaN", true, new String[] {"Inf/NaN"});

	static RecordType createFloat(String name, boolean dbl) {
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
		return new RecordType(name, RecordType.TupleCoding.BITPACK,
			new String[] {"Sign", "Exponent", "Mantissa"},
			new UserpType[] { TSign, exponent, significand });
	}
}

class ProtocolTypes {
	public static final ArrayType
		TTypeName= (ArrayType) CommonTypes.TSymbol.makeSynonym("TTypeName"),
		TMetadata= new ArrayType("TMetadata", ArrayType.TupleCoding.TIGHT, FundamentalTypes.TAny, TypeDef.VARIABLE_LEN),
		TEnumData= (ArrayType) TMetadata.makeSynonym("TEnumData");
//	static {
//		TMetadata.setStorage(new TypedArrayStorage_AllowNull(TMetadata));
//	}
}
