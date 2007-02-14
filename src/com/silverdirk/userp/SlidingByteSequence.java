package com.silverdirk.userp;

import java.io.*;
import java.util.*;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public class SlidingByteSequence extends ForwardOptimizedIndexedByteSequence {
	long headBlockAddr;
	long min;
	int count;
	InputStream source;
	int allocSize;

	public SlidingByteSequence(InputStream src) {
		this(src, 1024);
	}

	public SlidingByteSequence(InputStream src, int allocSize) {
		source= src;
		this.allocSize= allocSize;
		min= 0;
		count= 0;
		headBlockAddr= 0;
		appendEmptyBlock();
	}

	private final void appendEmptyBlock() {
		super.append(new byte[allocSize], (short)0, (short)0);
	}

	public long getMinAddr() {
		return min;
	}

	public long getMaxAddr() {
		return min+count-1;
	}

	public long getEndAddr() {
		return min+count;
	}

	public long getLength() {
		return count;
	}

	public ByteSequence subSequence(long from, long to) {
		return super.subSequence(from-headBlockAddr, to-headBlockAddr);
	}

	public void append(byte[] buffer, short from, short to) {
		throw new UnsupportedOperationException("Currently, only SlidingByteSequence may append data to itself (from the InputStream)");
		// the operation will involve copying from this buffer into the end of
		// the buffer of the last node, and possibly creating new nodes.
		// I don't need this functionality currently, so I'm not going to
		// implement it.
	}

	public IStream getStream(long address) {
		if (address < min || address > min+count)
			throw new IndexOutOfBoundsException("The address "+address+" is not within or at the end of the byte sequence ("+min+"..."+getMaxAddr()+")");
		return new SlidingIStream(address);
	}

	NodeAndAddress getNodeFor(long address) {
		NodeAndAddress result;
		// big special case for seeking to the next-to-be-written node
		if (address == getEndAddr()) {
			if (super.getLength() == 0)
				return new NodeAndAddress(address, getLast());
			result= super.getNodeFor(address-1-headBlockAddr);
			result.address+= headBlockAddr;
			// At this point, we're pointing to an invalid byte in the current buffer.
			// If there is a next buffer, thats where the next byte will go, so we switch to it.
			if (result.node.next != null) {
				result.address+= result.node.bufferEnd - result.node.bufferStart;
				result.node= result.node.next;
			}
		}
		else {
			result= super.getNodeFor(address-headBlockAddr);
			result.address+= headBlockAddr;
		}
		return result;
	}

	public void readMore() throws IOException {
		if (tryReadMore() < 0)
			throw new EOFException();
	}

	public void readMore(int minBytes) throws IOException {
		long goal= getMaxAddr()+minBytes;
		while (getMaxAddr() < goal)
			readMore();
	}

	public int tryReadMore() throws IOException {
		Node tail= getLast();
		int result= source.read(tail.buffer, tail.bufferEnd, tail.buffer.length-tail.bufferEnd);
		if (result != -1) {
			tail.bufferEnd+= result;
			tail.adjustLenCache(result);
			count+= result;
			if (tail.bufferEnd == tail.buffer.length)
				appendEmptyBlock();
		}
		return result;
	}

	public void discard(long qty) {
		if (qty > count)
			throw new IllegalArgumentException();
		min+= qty;
		count-= qty;
		NodeAndAddress newFirstNode= getNodeFor(min);
		headBlockAddr= newFirstNode.address;
		for (Node cur= getFirst(); cur != newFirstNode.node; cur= cur.next)
			cur.prune();
	}

	class SlidingIStream extends BlockIteratorIStream {
		public SlidingIStream(long addr) {
			super(addr);
		}

		protected boolean nextBlock() throws IOException {
			do {
				if (curBlock.bufferEnd != localLimit)
					super.nextBlock(curBlock, (short)localLimit);
				else if (curBlock.next != null)
					super.nextBlock(curBlock.next, (short)0);
				else if (tryReadMore() == -1)
					return false;
			} while (localPos == localLimit); // loop while the current buffer is exhausted
			return true;
		}
	}
}
