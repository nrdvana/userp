package com.silverdirk.userp;

import junit.framework.*;
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
public class _TestCodecs extends TestCase {
	private UserpEncoder enc;
	private UserpDecoder dec;
	private byte[] bytes;
	private Codec[] codecs;
	private ByteArrayOutputStream dest;

	protected void setUp() throws Exception {
		super.setUp();
	}

	private void prepEncoder(Codec[] codecs) {
		dest= new ByteArrayOutputStream();
		this.codecs= codecs;
		for (int i=0; i<codecs.length; i++)
			codecs[i].encoderTypeCode= i;
		enc= new UserpEncoder(dest, codecs);
	}

	private void prepDecoder() throws IOException {
		enc.flushToByteBoundary();
		bytes= dest.toByteArray();
		InputStream src= new ByteArrayInputStream(bytes);
		dec= new UserpDecoder(new BufferChainIStream(src), codecs);
	}

	protected void tearDown() throws Exception {
		super.tearDown();
		enc= null;
		dec= null;
		codecs= null;
	}

	public void testEnum64Codec() throws Exception {
		testEnum64Codec(false);
		testEnum64Codec(true);
	}

	private void testEnum64Codec(boolean bitpack) throws Exception {
		EnumType t1= new EnumType("t1").init(new Object[] {"foo", "bar", "baz"});
		Codec c1= new CodecCollection().getCodecFor(t1);
		prepEncoder(new Codec[] {c1});
		enc.enableBitpack(bitpack);
		new UserpWriter(enc, c1).write(0);
		new UserpWriter(enc, c1).write(1);
		new UserpWriter(enc, c1).write(2);
		try {
			new UserpWriter(enc, c1).write(3);
			assertTrue("Missed an expected exception", false);
		}
		catch (java.lang.IllegalArgumentException ex) {
		}
		// write an invalid value, so we can check our error handling
		enc.writeBits(3, 2);
		prepDecoder();
		byte[] expected= bytesFromHex(!bitpack? "00 01 02 03" : "1B");
		assertTrue(Arrays.equals(expected, bytes));
		dec.enableBitpack(bitpack);
		assertEquals(0, new UserpReader(dec, c1).readAsInt());
		assertEquals(1, new UserpReader(dec, c1).readAsInt());
		assertEquals(2, new UserpReader(dec, c1).readAsInt());
		try {
			new UserpReader(dec, c1).readAsInt();
			assertTrue("Missed an expected exception", false);
		}
		catch (UserpProtocolException ex) {
		}
	}

	public void testEnumInfCodec() throws Exception {
		testEnumInfCodec(false);
		testEnumInfCodec(true);
	}

	private void testEnumInfCodec(boolean bitpack) throws Exception {
		EnumType t1= new EnumType("t1").init(new Object[] {"foo", "bar", "baz", new EnumType.Range(Userp.TInt, 0, EnumType.Range.INF)});
		Codec c1= new CodecCollection().getCodecFor(t1);
		prepEncoder(new Codec[] {c1});
		enc.enableBitpack(bitpack);
		new UserpWriter(enc, c1).write(0);
		new UserpWriter(enc, c1).write(1);
		new UserpWriter(enc, c1).write(BigInteger.ONE.shiftLeft(1000));
		prepDecoder();
		dec.enableBitpack(bitpack);
		assertEquals(0, new UserpReader(dec, c1).readAsInt());
		assertEquals(1, new UserpReader(dec, c1).readAsInt());
		assertEquals(BigInteger.ONE.shiftLeft(1000), new UserpReader(dec, c1).readValue());
	}

	public void testUnionInfCodec() throws Exception {
		testUnionInfCodec(false, false);
		testUnionInfCodec(false, true);
		testUnionInfCodec(true, false);
		testUnionInfCodec(true, true);
	}

	private void testUnionInfCodec(boolean bitpack, boolean inline) throws Exception {
		EnumType t1= new EnumType("t1").init(new Object[] {"foo", "bar", "baz"});
		EnumType t2= new EnumType("t2").init(new Object[] {"foo", "bar", "baz", new EnumType.Range(Userp.TInt, 0, EnumType.Range.INF)});
		UnionType t3= new UnionType("t3").init(new UserpType[] {t1, Userp.TByte, Userp.TStrUTF8, t2});
		t3.setEncParams(bitpack, inline? new boolean[] {true, true, false, true} : new boolean[] {false, false, false, false});
		Codec c1= new CodecCollection().getCodecFor(t3);
		prepEncoder(new Codec[] {c1});
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(t1).write(0);
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(t1).write(2);
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(Userp.TByte).write(0);
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(Userp.TByte).write(255);
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(t2).write(0);
		enc.enableBitpack(false);
		new UserpWriter(enc, c1).select(t2).write(0x1234567);
		prepDecoder();
		byte[] expected= bytesFromHex(
			!bitpack? (!inline? "0000 0002 0100 01FF 0300 03C1234567" : "00 02 03 8102 8104 C123466B")
			// 00 00 / 00 02 / 01 00000000 / 01 11111111 / 11 00000000 / 1111 0000 0100 1000 1101 0001 0101 1001 11
			: (!inline? "00 20 4000 7FC0 C000 F048D159C0" : "00 02 03 8102 8104 C123466B")
		);
		assertTrue(Arrays.equals(expected, bytes));
		dec.enableBitpack(false);
		UserpReader r= new UserpReader(dec, c1);
		assertEquals(0, r.inspectUnion());
		assertEquals(0, r.readAsInt());
		dec.enableBitpack(false);
		r= new UserpReader(dec, c1);
		assertEquals(0, r.inspectUnion());
		assertEquals(2, r.readAsInt());
		dec.enableBitpack(false);
		r= new UserpReader(dec, c1);
		assertEquals(1, r.inspectUnion());
		assertEquals(0, r.readAsInt());
		dec.enableBitpack(false);
		r= new UserpReader(dec, c1);
		assertEquals(1, r.inspectUnion());
		assertEquals(255, r.readAsInt());
		dec.enableBitpack(false);
		r= new UserpReader(dec, c1);
		assertEquals(3, r.inspectUnion());
		assertEquals(0, r.readAsInt());
		dec.enableBitpack(false);
		r= new UserpReader(dec, c1);
		assertEquals(3, r.inspectUnion());
		assertEquals(0x1234567, r.readAsInt());
	}

	public void testPackedTupleCodec() throws Exception {
		testPackedTupleCodec(false);
		testPackedTupleCodec(true);
	}

	private void testPackedTupleCodec(boolean bitpack) throws Exception {
		ArrayType a1= new ArrayType("e1").init(Userp.TInt32);
		Codec c1= new CodecCollection().getCodecFor(a1);
		prepEncoder(new Codec[] {c1});
		enc.enableBitpack(bitpack);
		UserpWriter w= new UserpWriter(enc, c1);
		w.beginTuple(5);
		for (int i=0; i<5; i++)
			w.write(42);
		try {
			w.write(42);
			assertTrue("Missed an expected exception", false);
		}
		catch (java.lang.IllegalStateException ex) {
		}
		w.endTuple();
		prepDecoder();
		byte[] expected= bytesFromHex("05 0000002A 0000002A 0000002A 0000002A 0000002A");
		assertTrue(Arrays.equals(expected, bytes));
		dec.enableBitpack(bitpack);
		UserpReader r= new UserpReader(dec, c1);
		assertEquals(5, r.inspectTuple());
		for (int i=0; i<5; i++)
			assertEquals(42, r.readAsInt());
		try {
			r.readAsInt();
			assertTrue("Missed an expected exception", false);
		}
		catch (IllegalStateException ex) {
		}
		r.closeTuple();
	}

	public void testBitpackingInsideBytepackedTuple() throws Exception {
		EnumType e1= new EnumType("e1").init(new Object[] { new EnumType.Range(Userp.TInt, 0, 7) });
		ArrayType a2= new ArrayType("a2").init(e1).setEncParam(TupleCoding.BITPACK);
		ArrayType a1= new ArrayType("a1").init(a2).setEncParam(TupleCoding.PACK);
		Codec c1= new CodecCollection().getCodecFor(a1);
		prepEncoder(new Codec[] {c1});
		UserpWriter w= new UserpWriter(enc, c1);
		int[][] vals= new int[][] {
			new int[] { 1, 3, 5, 7, 2 },
			new int[] { 2, 5, 4, 3, 6, 7, 0 },
			new int[] { 7, 0, 7, 0, 7, 0, 7, 0, 7, 0 }
		};
		w.beginTuple(vals.length);
		for (int i=0; i<vals.length; i++) {
			w.beginTuple(vals[i].length);
			for (int j=0; j<vals[i].length; j++)
				w.write(vals[i][j]);
			w.endTuple();
		}
		w.endTuple();

		prepDecoder();
		// 3x 5x 001 011 101 111 010  7x 010 101 100 011 110 111 000  10x 111 000 111 000 111 000 111 000 111 000
		byte[] expected= bytesFromHex("03 052EF4 07563DC0 0AE38E38E0");
		assertTrue(Arrays.equals(expected, bytes));

		UserpReader r= new UserpReader(dec, c1);
		assertEquals(vals.length, r.inspectTuple());
		for (int i=0; i<vals.length; i++) {
			assertEquals(vals[i].length, r.inspectTuple());
			for (int j=0; j<vals[i].length; j++)
				assertEquals(vals[i][j], r.readAsInt());
			r.closeTuple();
		}
		r.closeTuple();
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
