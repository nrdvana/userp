package com.silverdirk.userp;

import junit.framework.*;
import java.io.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: SlidingByteSequence test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestBufferChainIStream extends TestCase {
	private BufferChainIStream in= null;
	private byte[] dataArray;

	protected void setUp() throws Exception {
		super.setUp();
		dataArray= new byte[4096];
		for (int i=0; i<dataArray.length; i++)
			dataArray[i]= (byte) i;
	}

	protected void tearDown() throws Exception {
		super.tearDown();
	}

	public void testPlainRead() throws Exception {
		for (int i=1; i<=5; i++)
			testPlainRead(i);
		for (int i=1022; i<=1026; i++)
			testPlainRead(i);
		testPlainRead(dataArray.length+5);
	}

	private void testPlainRead(int blockSize) throws Exception {
		in= new BufferChainIStream(new ByteArrayInputStream(dataArray), blockSize);
		byte[] buffer= new byte[128];
		int total= 0;
		while (in.getBytePos() < dataArray.length) {
			int red= in.read(buffer);
			assertTrue(red > 0);
			total+= red;
		}
		assertEquals(dataArray.length, total);
		assertEquals(dataArray.length, in.getBytePos());
		assertEquals(-1, in.read());
	}

	public void testForcedRead() throws IOException {
		for (int i=1; i<=5; i++)
			testForcedRead(i);
		for (int i=1022; i<=1026; i++)
			testForcedRead(i);
		testForcedRead(dataArray.length+5);
	}

	private void testForcedRead(int blockSize) throws IOException {
		in= new BufferChainIStream(new ByteArrayInputStream(dataArray), blockSize);
		byte[] buffer= new byte[128];
		int remaining= dataArray.length;
		while (in.getBytePos() < dataArray.length) {
			int count= Math.min(remaining, buffer.length);
			in.readFully(buffer, 0, count);
			remaining-= count;
		}
		assertEquals(0, remaining);
		assertEquals(dataArray.length, in.getBytePos());
		try {
			in.readUnsignedByte();
			assertTrue("missed an expected exception", false);
		}
		catch (EOFException ex) {
		}
	}
}
