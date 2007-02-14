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
public class _TestSlidingByteSequence extends TestCase {
	private SlidingByteSequence bseq= null;
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
		bseq= new SlidingByteSequence(new ByteArrayInputStream(dataArray), blockSize);
		int total= 0;
		while (total<dataArray.length) {
			int red= bseq.tryReadMore();
			assertTrue(red > 0);
			total+= red;
		}
		assertEquals(-1, bseq.tryReadMore());
	}

	public void testForcedRead() throws IOException {
		for (int i=1; i<=5; i++)
			testForcedRead(i);
		for (int i=1022; i<=1026; i++)
			testForcedRead(i);
		testForcedRead(dataArray.length+5);
	}
	private void testForcedRead(int blockSize) throws IOException {
		bseq= new SlidingByteSequence(new ByteArrayInputStream(dataArray), blockSize);
		while (bseq.getLength() < dataArray.length)
			bseq.readMore();
		assertEquals(dataArray.length, bseq.getLength());
		checkTreeIntegrity(bseq.getRoot());
		try {
			bseq.readMore();
			assertTrue("missed an expected exception", false);
		}
		catch (EOFException ex) {
		}
	}

	public void testReplayStreamCreationAndEOF() throws Exception {
		for (int i=1; i<=5; i++)
			testReplayStreamCreationAndEOF(i);
		for (int i=1022; i<=1026; i++)
			testReplayStreamCreationAndEOF(i);
		testReplayStreamCreationAndEOF(dataArray.length+5);
	}
	private void testReplayStreamCreationAndEOF(int blockSize) throws Exception {
		bseq= new SlidingByteSequence(new ByteArrayInputStream(dataArray), blockSize);
		for (int i=0; i<256; i++) {
			InputStream is= bseq.getStream(i);
			assertEquals(i, is.read());
		}
		checkTreeIntegrity(bseq.getRoot());
		long endAddr= bseq.getEndAddr();
		ByteSequence.IStream is= bseq.getStream(endAddr-1);
		assertEquals((endAddr-1)&0xFF, is.read());
		bseq.getStream(endAddr);
		try {
			bseq.getStream(endAddr+1);
			assertTrue("missed an expected exception", false);
		}
		catch (IndexOutOfBoundsException ex) {
		}
		while (bseq.getLength() != dataArray.length)
			bseq.readMore();
		is= bseq.getStream(bseq.getEndAddr());
		try {
			assertEquals(-1, is.read());
			is.forceRead();
			assertTrue("missed an expected exception", false);
		}
		catch (EOFException ex) {
		}
	}

	public void testDiscard() throws Exception {
		for (int j=1; j<10; j++) {
			for (int i=1; i<=5; i++)
				testDiscard(i, j);
			for (int i=1022; i<=1026; i++)
				testDiscard(i, j);
		}
	}
	private void testDiscard(int blockSize, int discardSize) throws Exception {
		bseq= new SlidingByteSequence(new ByteArrayInputStream(dataArray), blockSize);
		bseq.readMore(dataArray.length/2);
		int count= (int) bseq.getLength();
		for (int i=1; i<256 && bseq.getLength() > discardSize; i++) {
			bseq.discard(discardSize);
			assertEquals(i*discardSize, bseq.getMinAddr());
			assertEquals(count-i*discardSize, bseq.getLength());
			assertEquals(bseq.getMinAddr()&0xFF, bseq.getStream(bseq.getMinAddr()).read());
			try {
				bseq.getStream(i-1);
				assertTrue("missed an expected exception", false);
			}
			catch (Exception ex) {}
		}
		checkTreeIntegrity(bseq.getRoot());
		bseq.discard(bseq.getLength());
		assertEquals(0, bseq.getLength());
		bseq.readMore();
		InputStream is= bseq.getStream(bseq.getMinAddr());
		assertEquals(bseq.getMinAddr()&0xFF, is.read());
	}

	static int checkTreeIntegrity(ForwardOptimizedIndexedByteSequence.Node root) {
		return _TestForwardOptimizedIndexedByteSequence.checkTreeIntegrity(root);
	}
}
