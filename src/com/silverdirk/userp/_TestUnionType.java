package com.silverdirk.userp;

import junit.framework.*;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: UnionType test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestUnionType extends TestCase {
	static class TypeAndProps {
		UnionType t;
		int bits;
		long range;
		boolean isScalar;
		long[] offsets;

		TypeAndProps(UnionType rt, int bits, long range, boolean isScalar, long[] offsets) {
			this.t= rt;
			this.bits= bits;
			this.range= range;
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
			enum1= new EnumType("Enum 1", new String[] {"1-1"}),
			enum1_2= new EnumType("Enum 1 #2", new String[] {"1-1#2"}),
			enum2= new EnumType("Enum 2", new String[] {"1-2", "2-2"}),
			enum3= new EnumType("Enum 3", new String[] {"1-3", "2-3", "3-3"}),
			almostByte= new RangeType("AlmostByte", CommonTypes.TByte, 0, 254),
			int64= CommonTypes.TInt64;
		UnionType
			a= new UnionType("a", new UserpType[] {int8, whole}, new boolean[] {true, true} ),
			b= new UnionType("b", new UserpType[] {enum1, enum1_2}, new boolean[] {true, true} ),
			c= new UnionType("c", new UserpType[] {enum1, enum2, enum3, int8, whole}, new boolean[] {true, true, true, false, false} ),
			d= new UnionType("d", new UserpType[] {b, c, a}, new boolean[] { true, true, true }),
			e= new UnionType("e", new UserpType[] {almostByte, int64}, new boolean[] {true, false} );
		types= new TypeAndProps[] {
			new TypeAndProps(a, -1, -1, true, new long[] {0, 256}),
			new TypeAndProps(b, 1, 2, true, new long[] {0, 1}),
			new TypeAndProps(c, 3, 8, false, new long[] {0, 1, 3, 6, 7}),
			new TypeAndProps(d, -1, -1, false, new long[] {0, 2, 10}),
			new TypeAndProps(e, 8, 256, false, new long[] {0, 255}),
		};
		values= new Object[][] {
			new Object[] {
				new TypedData(int8, new Byte((byte)0x1)),
				new TypedData(int8, new Byte((byte)0x7F)),
				new TypedData(int8, new Byte((byte)-128)),
			},
			new Object[] {
				new TypedData(enum1, new Integer(0)),
				new TypedData(enum1_2, new Integer(0)),
			},
			new Object[] {
				new TypedData(enum3, new Integer(2)),
				new TypedData(whole, new Integer(0x12345)),
				new TypedData(int8, new Byte((byte)0xCA)),
			},
			new Object[] {
				new TypedData(b, new TypedData(enum1_2, new Integer(0))),
				new TypedData(a, new TypedData(whole, new Integer(0x12345))),
			},
			new Object[] {
				new TypedData(almostByte, new Byte((byte)0)),
				new TypedData(almostByte, new Byte((byte)0xFE)),
				new TypedData(almostByte, new Byte((byte)0x7F)),
				new TypedData(int64, new Long(-1)),
			},
		};
		encodings= new byte[][] {
			bytesFromHex("01 7F 8080"),
			bytesFromHex("00 01"),
			bytesFromHex("05 07A12345 06CA"),
			bytesFromHex("01 A1244F"), // 0xA0[0000] | (0x12345 + 0x100 + 0xA)
			bytesFromHex("00 FE 7F FFFFFFFFFFFFFFFFFF"),
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
			assertTrue(msg, types[i].t.hasScalarComponent());
			assertEquals(msg, types[i].isScalar, types[i].t.isScalar());
			assertEquals(msg, types[i].bits, types[i].t.getScalarBitLen());
			assertEquals(msg, BigInteger.valueOf(types[i].range), types[i].t.getScalarRange());
			for (int j=0; j<types[i].offsets.length; j++)
				assertEquals(types[i].offsets[j], types[i].t.unionDef.offsets_l[j]);
			UnionType aClone= (UnionType) types[i].t.makeSynonym(types[i].t.getMeta());
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
				assertEquals(msg, values[i][j], reader.readValue());
			}
		}
	}
	static final byte[] bytesFromHex(String hexStr) {
		return _TestCommonTypes.bytesFromHex(hexStr);
	}
}
