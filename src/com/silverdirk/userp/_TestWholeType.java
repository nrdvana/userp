package com.silverdirk.userp;

import junit.framework.*;
import java.io.ByteArrayInputStream;
import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: WholeType test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestWholeType extends TestCase {
	UserpType[] types;
	Object[][] values;
	byte[][] encodings;

	protected void setUp() throws Exception {
		super.setUp();
		types= new UserpType[] {
			FundamentalTypes.TWhole,
			FundamentalTypes.TNegWhole
		};
		values= new Object[][] {
			new Object[] {
				new Integer(0), new Integer(127),  new Integer(128),
				new Integer(Integer.MAX_VALUE), new Long(Integer.MAX_VALUE+1L),
				new Long(Long.MAX_VALUE), BigInteger.valueOf(Long.MAX_VALUE).add(BigInteger.ONE),
			},
			new Object[] {
				new Integer(0), new Integer(-127), new Integer(-128),
				new Integer(Integer.MIN_VALUE+1), new Integer(Integer.MIN_VALUE),
				new Long(Long.MIN_VALUE+1), new Long(Long.MIN_VALUE),
			},
		};
		encodings= new byte[][] {
			new byte[] {
				0,
				0x7F,
				(byte)(1<<7),(byte)0x80,
				(byte)(7<<5)|0,0x7F,(byte)0xFF,(byte)0xFF,(byte)0xFF,
				(byte)(7<<5)|0,(byte)0x80,0x00,0x00,0x00,
				(byte)(7<<5)|4,0x7F,(byte)0xFF,(byte)0xFF,(byte)0xFF,(byte)0xFF,(byte)0xFF,(byte)0xFF,(byte)0xFF,
				(byte)(7<<5)|4,(byte)0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
			},
			null
		};
		encodings[1]= encodings[0];
	}

	protected void tearDown() throws Exception {
		types= null;
		values= null;
		encodings= null;
		super.tearDown();
	}

	public void testTypeProperties() {
		for (int i=0; i<types.length; i++) {
			String msg= "for type "+types[i].getName();
			assertTrue(msg, types[i].hasScalarComponent());
			assertTrue(msg, types[i].isScalar());
			assertEquals(msg, TypeDef.VARIABLE_LEN, types[i].getScalarBitLen());
			assertEquals(msg, TypeDef.INFINITE, types[i].getScalarRange());
			WholeType aClone= (WholeType) types[i].makeSynonym(types[i].getMeta());
			assertEquals(msg, types[i], aClone);
			assertEquals(msg, types[i].hashCode(), aClone.hashCode());
			assertFalse(msg, types[i].handle.equals(aClone.handle));
		}
	}

	public void testEncode() {
	}

	public void testDecode() throws Exception {
		for (int i=0; i<types.length; i++) {
			String msg= "for type "+types[i].getName();
			ByteSequence bseq= new SlidingByteSequence(new ByteArrayInputStream(encodings[i]));
			UserpReader reader= new UserpReader(bseq, bseq.getStream(0), UserpFileReader.rootTypes);
			for (int j=0; j<values[i].length; j++) {
				reader.startFreshElement(types[i]);
				assertEquals(msg, values[i][j], reader.readValue());
			}
		}
	}
}
