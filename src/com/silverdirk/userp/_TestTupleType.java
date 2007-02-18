package com.silverdirk.userp;

import junit.framework.*;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;
import java.util.Arrays;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: TupleType test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestTupleType extends TestCase {
	static class TypeAndProps {
		TupleType t;
		boolean hasScalar;
		boolean isScalar;
		long[] offsets;

		TypeAndProps(TupleType t, boolean hasScalar, boolean isScalar) {
			this.t= t;
			this.hasScalar= hasScalar;
			this.isScalar= isScalar;
			this.offsets= offsets;
		}
	}

	TypeAndProps[] types;
	Object[][] values;
	byte[][] encodings;

	protected void setUp() throws Exception {
		super.setUp();
		UserpType
			whole= FundamentalTypes.TWhole,
			int32= CommonTypes.TInt32,
			int8= CommonTypes.TInt8,
			int4= new RangeType("nibble", whole, 0, 7),
			int16= CommonTypes.TInt16,
			almostByte= new RangeType("AlmostByte", CommonTypes.TByte, 0, 254),
			int64= CommonTypes.TInt64,
			byteOrLong= new UnionType("ByteOrLong", new UserpType[] {almostByte, int64}, new boolean[] {true, false} );
		types= new TypeAndProps[] {
			new TypeAndProps(new ArrayType("a", byteOrLong, 4), true, false),
			new TypeAndProps(new ArrayType("bits", TupleType.TupleCoding.BITPACK, CommonTypes.TBool, ArrayType.VARIABLE_LEN), true, false),
			new TypeAndProps(new ArrayType("c", TupleType.TupleCoding.INDEXED, CommonTypes.TInt32, ArrayType.VARIABLE_LEN), true, false),
			new TypeAndProps(new ArrayType("d", TupleType.TupleCoding.INDEXED, CommonTypes.TInt32, 5), false, false),
			new TypeAndProps(new ArrayType("e", TupleType.TupleCoding.INDEFINITE, CommonTypes.TInt32, 5), false, false),
		};
		values= new Object[][] {
			new Object[] {
				new Object[] {new TypedData(almostByte, new Byte((byte)12)), new TypedData(int64, new Long(0x12345)),
					new TypedData(int64, new Long(-1)), new TypedData(almostByte, new Byte((byte)0))
				}
			},
			new Object[] {
			    new boolean[] { true },
				new boolean[] { true, true, false, true, false, false },
				new boolean[] { true, true, true, true, true, true, true, true, true, true, true, true, true, true, true },
			},
			new Object[] {
				new int[] { 0x12345678 },
				new int[] { 0x12345678, 0x12345678, 0x12345678 },
				new int[] { 0x12345678, 0x12345678, 0x12345678 },
			},
			new Object[] {
				new int[] { 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678 },
			},
			new Object[] {
				new int[] { 0x12345678, 0x12345678, 0x12345678, 0x12345678, 0x12345678 },
			}
		};
		encodings= new byte[][] {
			bytesFromHex("0C FF0000000000012345 FFFFFFFFFFFFFFFFFF 00"),
			bytesFromHex("01 80   06 D0   0F FFFE"),
			bytesFromHex("01 01 05 00 12345678"
				+"03 03 050505 00 12345678 00 12345678 00 12345678"
				+"03 05 0507060000 00 12345678 00 123456780000 00 1234567800"),
			bytesFromHex("05 0505050505 00 12345678 00 12345678 00 12345678 00 12345678 00 12345678"),
			bytesFromHex("01 12345678 01 12345678 01 12345678 01 12345678 01 12345678 00"),
		};
	}

	protected void tearDown() throws Exception {
		types= null;
		values= null;
		encodings= null;
		super.tearDown();
	}

	public void testTypeProperties() {
		for (int i=0; i<types.length; i++) {
			String msg= "for type "+types[i].t.getName();
			assertEquals(msg, types[i].hasScalar, types[i].t.hasScalarComponent());
			assertEquals(msg, types[i].isScalar, types[i].t.isScalar());
			if (types[i].hasScalar) {
				if (types[i].t.getElemCount() == -1) {
					assertEquals(msg, TypeDef.VARIABLE_LEN, types[i].t.getScalarBitLen());
					assertEquals(msg, TypeDef.INFINITE, types[i].t.getScalarRange());
				}
				else {
					assertEquals(msg, types[i].t.getElemType(0).getScalarBitLen(), types[i].t.getScalarBitLen());
					assertEquals(msg, types[i].t.getElemType(0).getScalarRange(), types[i].t.getScalarRange());
				}
			}
			UserpType aClone= (UserpType) types[i].t.makeSynonym(types[i].t.getMeta());
			assertEquals(msg, types[i].t, aClone);
			assertEquals(msg, types[i].t.hashCode(), aClone.hashCode());
			assertFalse(msg, types[i].t.handle.equals(aClone.handle));
		}
	}

	public void testEncode() {
	}

	public void testDecode() throws Exception {
		for (int i=0; i<types.length; i++) {
			String msg= "for type "+types[i].t.getName();
			ByteSequence bseq= new SlidingByteSequence(new ByteArrayInputStream(encodings[i]));
			UserpReader reader= new UserpReader(bseq, bseq.getStream(0), UserpFileReader.rootTypes);
			for (int j=0; j<values[i].length; j++) {
				reader.startFreshElement(types[i].t);
				assertTrue(msg, Util.arrayEquals(true, values[i][j], reader.readValue()));
			}
		}
	}

	static final byte[] bytesFromHex(String hexStr) {
		return _TestCommonTypes.bytesFromHex(hexStr);
	}
}
