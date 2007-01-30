package com.silverdirk.userp;

import junit.framework.*;
import java.io.*;

/**
 * <p>Project: </p>
 *
 * <p>Title: </p>
 *
 * <p>Description: </p>
 *
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public class _TestForwardOptimizedIndexedByteSequence extends TestCase {
	private ForwardOptimizedIndexedByteSequence bseq= null;
	byte[] data;

	protected void setUp() throws Exception {
		super.setUp();
		data= new byte[2048];
		for (int i=0; i<data.length; i++)
			data[i]= (byte) i;
		bseq= new ForwardOptimizedIndexedByteSequence();
	}

	protected void tearDown() throws Exception {
		super.tearDown();
	}

	public void testAppend() throws Exception {
		short[] divs= new short[] { 0, 1, 2, 4, 7, 11, 20, 50, 127, 128, 129, (short)data.length };
		for (int i=1; i<divs.length; i++) {
			checkTreeIntegrity(bseq.getRoot());
			bseq.append(data, divs[i-1], divs[i]);
		}
		checkTreeIntegrity(bseq.getRoot());
		assertEquals(data.length, bseq.getLength());
		ByteSequence.IStream stream= bseq.getStream(0);
		for (int i=0; i<data.length; i++)
			assertEquals(data[i], stream.forceRead());
		try {
			stream.forceRead();
			assertTrue("Missed an expected exception", false);
		}
		catch (EOFException ex) {}
	}

	static int checkTreeIntegrity(ForwardOptimizedIndexedByteSequence.Node root) {
		if (root.isSentinel())
			return 0;
		int leftBlackDepth= checkTreeIntegrity(root.left);
		int rightBlackDepth= checkTreeIntegrity(root.right);
		assertEquals(leftBlackDepth, rightBlackDepth);
		assertEquals(root.left.getLength() + root.right.getLength() + (root.bufferEnd-root.bufferStart), root.getLength());
		if (!root.left.isSentinel())
			assertEquals(root.traverseToPrev().next, root);
		if (!root.right.isSentinel())
			assertEquals(root.next, root.traverseToNext());
		if (root.getColor() == root.Red) {
			assertEquals(root.Black, root.left.getColor());
			assertEquals(root.Black, root.right.getColor());
			return leftBlackDepth;
		}
		else
			return leftBlackDepth+1;
	}
}
