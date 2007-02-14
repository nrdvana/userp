package com.silverdirk.userp;

import junit.framework.*;
import java.math.BigInteger;
import java.io.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: RangeType test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestRangeType extends TestCase {
	static class TypeAndProps {
		RangeType t;
		int bits;
		long range;

		TypeAndProps(RangeType rt, int bits, long range) {
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
			new TypeAndProps(new RangeType("a", FundamentalTypes.TWhole, 0, 15), 4, 16),
			new TypeAndProps(new RangeType("b", FundamentalTypes.TWhole, 0, 16), 5, 17),
			new TypeAndProps(new RangeType("c", FundamentalTypes.TWhole, 16, 0), 5, 17),
			new TypeAndProps(new RangeType("d", FundamentalTypes.TWhole, 16, 143), 7, 128),
			new TypeAndProps(new RangeType("e", FundamentalTypes.TWhole, BigInteger.valueOf(16), BigInteger.valueOf(143)), 7, 128),
			new TypeAndProps(new RangeType("f", FundamentalTypes.TWhole, BigInteger.ONE, RangeType.RangeDef.INFINITE), -1, -1),
			new TypeAndProps(new RangeType("g", FundamentalTypes.TWhole, 1, 100000), 17, 100000),
		};
		values= new Object[][] {
			new Object[0],
			new Object[] {new Integer(5), new Integer(16), new Integer(0)},
			new Object[] {new Integer(16), new Integer(0)},
			new Object[] {new Integer(16), new Integer(143)},
			new Object[] {new Integer(16), new Integer(143)},
			new Object[] {new Integer(1), new Integer(100000)},
			new Object[] {new Integer(1), new Integer(100000)},
		};
		encodings= new byte[][] {
			new byte[0],
			new byte[] {5, 16, 0},
			new byte[] {0, 16},
			new byte[] {0, 127},
			new byte[] {0, 127},
			new byte[] {0, (byte)(1<<7)|(1<<5)|1,(byte)0x86,(byte)0x9F},
			new byte[] {0,0,0, 1,(byte)0x86,(byte)0x9F},
		};
	}

	protected void tearDown() throws Exception {
		types= null;
		values= null;
		encodings= null;
		super.tearDown();
	}

	public void testRangeProperties() {
		for (int i=0; i<types.length; i++) {
			String msg= "for type "+types[i].t.getName();
			assertTrue(msg, types[i].t.hasScalarComponent());
			assertTrue(msg, types[i].t.isScalar());
			assertEquals(msg, types[i].bits, types[i].t.getScalarBitLen());
			assertEquals(msg, BigInteger.valueOf(types[i].range), types[i].t.getScalarRange());
			RangeType aClone= (RangeType) types[i].t.makeSynonym(types[i].t.getMeta());
			assertEquals(msg, types[i].t, aClone);
			assertEquals(msg, types[i].t.hashCode(), aClone.hashCode());
			assertFalse(msg, types[i].t.handle.equals(aClone.handle));
		}
	}

	public void testRangeEncode() {
	}

	public void testRangeDecode() throws Exception {
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
}
