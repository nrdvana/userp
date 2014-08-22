package com.silverdirk.userp;

import java.io.*;
import java.util.*;
import java.math.BigInteger;
import com.silverdirk.userp.EnumType.Range;
import com.silverdirk.userp.ValRegister.StoType;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class Userp {
	public static final IntegerType TInt;
	public static final EnumType TNull, TWhole, TBool, TByte, TInt8, TInt16u, TInt16, TInt32u, TInt32, TInt64u, TInt64;
	public static final ArrayType TBoolArray, TByteArray, TSymbol, TStrUTF8, TStrUTF16;
	public static final RecordType
		TFloat32= null,
		TFloat64= null;
	public static final UserpType TAny;

	static final UserpType[] predefTypes;

	static {
		TAny= AnyType.INSTANCE;
		TInt= new IntegerType("TInt");
		TWhole= new EnumType("TWhole").init(new Object[] {new Range(TInt, 0, Range.INF)});
		TNull= new EnumType("TNull").init(new Object[] {"Null"});
		TNull.setCustomCodec(NullConverter.INSTANCE);
		TBool= new EnumType("TBool").init(new Object[] {"False","True"});
		TByte= new EnumType("TByte").init(new Object[] {new Range(TInt, 0, 0xFF)});
		TByte.setCustomCodec(new NativeTypeConverter(StoType.BYTE));
		TInt8= new EnumType("TInt8").init(new Object[] {new Range(TInt, 0, 0x7F), new Range(TInt, -0x80, -1)});
		TInt8.setCustomCodec(new NativeTypeConverter(StoType.BYTE));
		TInt16u= new EnumType("TInt16u").init(new Object[] {new Range(TInt, 0, 0xFFFF)});
		TInt16u.setCustomCodec(new NativeTypeConverter(StoType.SHORT));
		TInt16=  new EnumType("TInt16" ).init(new Object[] {new Range(TInt, 0, 0x7FFF), new Range(TInt, -0x8000, -1)});
		TInt16.setCustomCodec(new NativeTypeConverter(StoType.SHORT));
		TInt32u= new EnumType("TInt32u").init(new Object[] {new Range(TInt, 0, 0xFFFFFFFFL)});
		TInt32u.setCustomCodec(new NativeTypeConverter(StoType.INT));
		TInt32=  new EnumType("TInt32" ).init(new Object[] {new Range(TInt, 0, 0x7FFFFFFF), new Range(TInt, -0x80000000, -1)});
		TInt32.setCustomCodec(new NativeTypeConverter(StoType.INT));
		TInt64u= new EnumType("TInt64u").init(new Object[] {new Range(TInt, BigInteger.ZERO, BigInteger.ONE.shiftLeft(64).subtract(BigInteger.ONE))});
		TInt64u.setCustomCodec(new NativeTypeConverter(StoType.LONG));
		TInt64=  new EnumType("TInt64" ).init(new Object[] {new Range(TInt, 0, 0x7FFFFFFFFFFFFFFFL), new Range(TInt, -0x8000000000000000L, -1)});
		TInt64.setCustomCodec(new NativeTypeConverter(StoType.LONG));
		TBoolArray= new ArrayType("TBoolArray").init(TBool);
		TByteArray= new ArrayType("TByteArray").init(TByte);
		TSymbol= (ArrayType) TByteArray.cloneAs("TSymbol");
		TSymbol.setCustomCodec(SymbolConverter.INSTANCE);
		TStrUTF8= (ArrayType) TByteArray.cloneAs("TStrUTF8");
		TStrUTF16= (ArrayType) TByteArray.cloneAs("TStrUTF16");
		predefTypes= new UserpType[] {
			TNull, TInt, TWhole, TByte, TInt8, TInt16u, TInt16, TInt32u, TInt32,
			TInt64u, TInt64, TFloat32, TFloat64, TBoolArray, TByteArray, TSymbol,
			TStrUTF8, TStrUTF16
		};
	}

	public static UserpReader createStdReader(InputStream src) {
		//	static final Codec[] PREDEF_DECODERS;
		//	static {
		//		HashMap<UserpType.TypeHandle,Codec> typeToDecoder= new HashMap<UserpType.TypeHandle,Codec>();
		//		PREDEF_DECODERS= new Codec[Userp.predefTypes.length];
		//		for (int i=0,limit=Userp.predefTypes.length; i<limit; i++)
		//			PREDEF_DECODERS[i]= Codec.forType(Userp.predefTypes[i], typeToDecoder);
		//	}
		throw new Error("Unimplemented");
	}

	public static UserpWriter createStdWriter(OutputStream dest) {
		throw new Error("Unimplemented");
	}
}
