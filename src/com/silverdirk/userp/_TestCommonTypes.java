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

		TypeAndProps(UserpType rt, int bits, long range) {
			this(rt, bits, BigInteger.valueOf(range));
		}
		TypeAndProps(UserpType rt, int bits, BigInteger range) {
			this.t= rt;
			this.bits= bits;
			this.range= range;
		}
	}

	TypeAndProps[] types;
	Object[][] values;
	byte[][] encodings;

	protected void setUp() throws Exception {
		super.setUp();
		types= new TypeAndProps[] {
			new TypeAndProps(CommonTypes.TInt8, 8, 1L<<8),
			new TypeAndProps(CommonTypes.TInt16, 16, 1L<<16),
			new TypeAndProps(CommonTypes.TInt32, 32, 1L<<32),
			new TypeAndProps(CommonTypes.TInt64, 64, BigInteger.ONE.shiftLeft(64)),
			new TypeAndProps(CommonTypes.TByte, 8, 1L<<8),
			new TypeAndProps(CommonTypes.TInt16u, 16, 1L<<16),
			new TypeAndProps(CommonTypes.TInt32u, 32, 1L<<32),
			new TypeAndProps(CommonTypes.TInt64u, 64, BigInteger.ONE.shiftLeft(64)),
		};
		values= new Object[][] {
			new Object[] { new Byte((byte)0), new Byte((byte)127), new Byte((byte)-128), },
			new Object[] { new Short((short)0), new Short((short)0x7FFF), new Short((short)0x8000), },
			new Object[] { new Integer(0), new Integer(0x7FFFFFFF), new Integer(-0x80000000), },
			new Object[] { new Long(0), new Long(0x7FFFFFFFFFFFFFFFL), new Long(-0x8000000000000000L), },

			new Object[] { new Byte((byte)0), new Byte((byte)127), new Byte((byte)-128), },
			new Object[] { new Short((short)0), new Short((short)0x7FFF), new Short((short)0x8000), },
			new Object[] { new Integer(0), new Integer(0x7FFFFFFF), new Integer(-0x80000000), },
			new Object[] { new Long(0), new Long(0x7FFFFFFFFFFFFFFFL), new Long(0xFFFFFFFFFFFFFFFFL), },
		};
		encodings= new byte[][] {
			bytesFromHex("00 7F 80"),
			bytesFromHex("0000 7FFF 8000"),
			bytesFromHex("00000000 7FFFFFFF 80000000"),
			bytesFromHex("0000000000000000 7FFFFFFFFFFFFFFF 8000000000000000"),

			bytesFromHex("00 7F 80"),
			bytesFromHex("0000 7FFF 8000"),
			bytesFromHex("00000000 7FFFFFFF 80000000"),
			bytesFromHex("0000000000000000 7FFFFFFFFFFFFFFF FFFFFFFFFFFFFFFF"),
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
			assertTrue(msg, types[i].t.isScalar());
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
				assertEquals(msg, values[i][j], reader.readValue());
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
		return (byte) (((ch1<<4)&0xF0) | (ch2&0x0F));
	}
}
