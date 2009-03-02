package com.silverdirk.userp;

import junit.framework.*;
import java.io.*;
import java.util.*;
import java.math.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: SlidingByteSequence test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestUserpEncoderDecoder extends TestCase {
	private UserpEncoder enc= null;
	private UserpDecoder dec= null;
	private ByteArrayOutputStream dest= null;

	protected void setUp() throws Exception {
		super.setUp();
		dest= new ByteArrayOutputStream();
		enc= new UserpEncoder(dest, new Codec[0]);
	}

	private void prepDecoder() throws IOException {
		enc.flush();
		InputStream src= new ByteArrayInputStream(dest.toByteArray());
		dec= new UserpDecoder(new BufferChainIStream(src), new CodecDescriptor[0]);
	}

	protected void tearDown() throws Exception {
		super.tearDown();
		enc= null;
		dec= null;
	}

	// generate a 1-bit every 3 bit positions
	protected long nextInSeq(long prev) {
		return (prev<<1) | (1-((prev|(prev>>1))&0x1));
	}

	// generate a 1-bit every 3 bit positions
	protected BigInteger nextInSeq(BigInteger prev) {
		BigInteger result= prev.shiftLeft(1);
		if (!prev.testBit(0) && !prev.testBit(1))
			result= result.setBit(0);
		return result;
	}

	public void testVarQty() throws Exception {
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			if (val.bitLength() < 64)
				enc.writeVarQty(val.longValue());
			else
				enc.writeVarQty(val);
		}
		prepDecoder();
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			String msg= "@"+val.bitLength();
			dec.readVarQty();
			if (val.bitLength() < 64)
				assertEquals(msg, val.longValue(), dec.scalarAsLong());
			else
				assertEquals(msg, val, dec.scalarAsBigInt());
		}
	}

	public void testQty() throws Exception {
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			if (val.bitLength() < 64)
				enc.writeQty(val.longValue(), val.bitLength());
			else
				enc.writeQty(val, val.bitLength());
		}
		prepDecoder();
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			String msg= "@"+val.bitLength();
			dec.readQty(val.bitLength());
			if (val.bitLength() < 64)
				assertEquals(msg, val.longValue(), dec.scalarAsLong());
			else
				assertEquals(msg, val, dec.scalarAsBigInt());
		}
	}

	public void testVarQty_bitpacked() throws Exception {
		enc.enableBitpack(true);
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			if (val.bitLength() < 64)
				enc.writeVarQty(val.longValue());
			else
				enc.writeVarQty(val);
		}
		prepDecoder();
		dec.enableBitpack(true);
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			String msg= "@"+val.bitLength();
			dec.readVarQty();
			if (val.bitLength() < 64)
				assertEquals(msg, val.longValue(), dec.scalarAsLong());
			else
				assertEquals(msg, val, dec.scalarAsBigInt());
		}
	}

	public void testQty_bitpacked() throws Exception {
		enc.enableBitpack(true);
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			if (val.bitLength() < 64)
				enc.writeQty(val.longValue(), val.bitLength());
			else
				enc.writeQty(val, val.bitLength());
		}
		prepDecoder();
		dec.enableBitpack(true);
		for (BigInteger val= BigInteger.ZERO; val.bitLength()<4096; val= nextInSeq(val)) {
			String msg= "@"+val.bitLength();
			dec.readQty(val.bitLength());
			if (val.bitLength() < 64)
				assertEquals(msg, val.longValue(), dec.scalarAsLong());
			else
				assertEquals(msg, val, dec.scalarAsBigInt());
		}
	}
}
