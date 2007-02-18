package com.silverdirk.userp;

import junit.framework.*;
import java.math.BigInteger;
import java.io.ByteArrayInputStream;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: CommonTypes test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestCommonTypes extends TestCase {
	static class TypeAndProps {
		UserpType t;
		int bits;
		BigInteger range;
		boolean hasScalar, isScalar;

		TypeAndProps(UserpType rt, int bits, long range, boolean hasScalar, boolean isScalar) {
			this(rt, bits, BigInteger.valueOf(range), hasScalar, isScalar);
		}
		TypeAndProps(UserpType rt, int bits, BigInteger range, boolean hasScalar, boolean isScalar) {
			this.t= rt;
			this.bits= bits;
			this.range= range;
			this.hasScalar= hasScalar;
			this.isScalar= isScalar;
		}
	}

	TypeAndProps[] types;
	Object[][] values;
	byte[][] encodings;

	protected void setUp() throws Exception {
		super.setUp();
		types= new TypeAndProps[] {
			new TypeAndProps(CommonTypes.TBool, 1, 2, true, true),

			new TypeAndProps(CommonTypes.TInt8, 8, 1L<<8, true, true),
			new TypeAndProps(CommonTypes.TInt16, 16, 1L<<16, true, true),
			new TypeAndProps(CommonTypes.TInt32, 32, 1L<<32, true, true),
			new TypeAndProps(CommonTypes.TInt64, 64, BigInteger.ONE.shiftLeft(64), true, true),

			new TypeAndProps(CommonTypes.TByte, 8, 1L<<8, true, true),
			new TypeAndProps(CommonTypes.TInt16u, 16, 1L<<16, true, true),
			new TypeAndProps(CommonTypes.TInt32u, 32, 1L<<32, true, true),
			new TypeAndProps(CommonTypes.TInt64u, 64, BigInteger.ONE.shiftLeft(64), true, true),

			new TypeAndProps(CommonTypes.TInt, -1, -1, true, false),

			new TypeAndProps(CommonTypes.TFloat32, 1, 2, true, false),
			new TypeAndProps(CommonTypes.TFloat64, 1, 2, true, false),

			new TypeAndProps(CommonTypes.TByteArray, -1, -1, true, false),
			new TypeAndProps(CommonTypes.TStrUTF8, -1, -1, true, false),
			new TypeAndProps(CommonTypes.TStrUTF16, -1, -1, true, false),
		};
		values= new Object[][] {
			new Object[] { Boolean.TRUE, Boolean.FALSE },

			new Object[] { new Byte((byte)0), new Byte((byte)127), new Byte((byte)-128), },
			new Object[] { new Short((short)0), new Short((short)0x7FFF), new Short((short)0x8000), },
			new Object[] { new Integer(0), new Integer(0x7FFFFFFF), new Integer(-0x80000000), },
			new Object[] { new Long(0), new Long(0x7FFFFFFFFFFFFFFFL), new Long(-0x8000000000000000L), },

			new Object[] { new Byte((byte)0), new Byte((byte)127), new Byte((byte)-128), },
			new Object[] { new Short((short)0), new Short((short)0x7FFF), new Short((short)0x8000), },
			new Object[] { new Integer(0), new Integer(0x7FFFFFFF), new Integer(-0x80000000), },
			new Object[] { new Long(0), new Long(0x7FFFFFFFFFFFFFFFL), new Long(0xFFFFFFFFFFFFFFFFL), },

			new Object[] { BigInteger.valueOf(0x1234), BigInteger.valueOf(-0x1234) },

			new Object[] { new Float(0), new Float(1.625) },
			new Object[] { new Double(0), new Double(1.625) },

			new Object[] { bytesFromHex("01 02 03 04 05 7F 80 81 99 FF") },

			new Object[] { "Foo", "", "\u8B58" /* Kanji, chosen at random from charmap.*/ },
			new Object[] { "Foo", "", "\u8B58" },
		};
		encodings= new byte[][] {
			bytesFromHex("01 00"),

			bytesFromHex("00 7F 80"),
			bytesFromHex("0000 7FFF 8000"),
			bytesFromHex("00000000 7FFFFFFF 80000000"),
			bytesFromHex("0000000000000000 7FFFFFFFFFFFFFFF 8000000000000000"),

			bytesFromHex("00 7F 80"),
			bytesFromHex("0000 7FFF 8000"),
			bytesFromHex("00000000 7FFFFFFF 80000000"),
			bytesFromHex("0000000000000000 7FFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF"),

			bytesFromHex("9234 009234"),

			bytesFromHex("00000000 3FD00000"), // 0 01111111 10100000000000000000000
			bytesFromHex("0000000000000000 3FFA000000000000"), // 0 01111111111 1010000000000000000000000000000000000000000000000000

			bytesFromHex("0A 01 02 03 04 05 7F 80 81 99 FF"),

			bytesFromHex("03466F6F  00  03E8AD98"),
			bytesFromHex("030046006F006F  00  018B58"),
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
			assertEquals(msg, types[i].bits, types[i].t.getScalarBitLen());
			assertEquals(msg, types[i].range, types[i].t.getScalarRange());
			UserpType aClone= types[i].t.makeSynonym(types[i].t.getMeta());
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
				Object val= reader.readValue();
				if (val.getClass().isArray())
					assertTrue(Util.arrayEquals(true, values[i][j], val));
				else
					assertEquals(msg, values[i][j], val);
			}
		}
	}

	static final byte[] bytesFromHex(String hexStr) {
		char[] chars= hexStr.toCharArray();
		int digits= 0;
		for (int i=0; i<chars.length; i++)
			if (chars[i] != ' ')
				digits++;
		byte[] result= new byte[digits>>1];
		int pos= 0;
		int highDigit= 0;
		for (int i=0; pos<digits; i++)
			if (chars[i] != ' ') {
				if ((pos & 1) == 0)
					highDigit= i;
				else
					result[pos>>1]= hexCharsToValue(chars[highDigit], chars[i]);
				pos++;
			}
		return result;
	}
	static final byte hexCharsToValue(char ch1, char ch2) {
		if (ch1 > 0x40)
			ch1+= 9;
		if (ch2 > 0x40)
			ch2+= 9;
		return (byte) (((ch1&0x0F)<<4) | (ch2&0x0F));
	}
}
