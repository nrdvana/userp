package com.silverdirk.userp;

import java.io.*;

/**
 * <p>Project: Universal Serialization Protocol Reference Library</p>
 * <p>Title: Buffer-Chain Input Stream</p>
 * <p>Description: A buffered input stream that uses a linked list of buffers, and can be cloned</p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * The main purpose of BufferChainIStream is to be able to clone a stream such
 * that reading from either stream provides the same sequence of data.  If any
 * clone of the stream reaches the end of the buffer, it will pull more form
 * the underlying stream.  Each clone can safely be used by a different thread,
 * though an individual clone should be used by only one thread at a time.
 *
 * This is essentially the same as the "mark" feature of InputStream, but can
 * be applied any number of times, and is a more object-oriented approach.
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class BufferChainIStream extends InputStream implements Cloneable {
	static class Block {
		Block next;
		short bufferPos;
		byte[] buffer;
	}

	final int
		BUFFER_SIZE;
	static final int
		MIN_READ_SIZE= 64;

	BufferChainIStream markedState;

	InputStream source;
	Block curBlock;
	int localPos;
	int localLimit;
	long limitByteAddress;

	public BufferChainIStream(InputStream source) {
		this(source, 1024);
	}

	public BufferChainIStream(InputStream source, int bufferSize) {
		this(source, bufferSize, 0);
	}

	public BufferChainIStream(InputStream source, int bufferSize, long bytePosStartValue) {
		BUFFER_SIZE= bufferSize;
		this.source= source;
		curBlock= newBlock();
		localPos= 0;
		localLimit= 0;
		limitByteAddress= bytePosStartValue;
	}

	public Object clone() {
		try {
			BufferChainIStream result= (BufferChainIStream) super.clone();
			if (result.markedState != null)
				result.markedState= (BufferChainIStream) result.markedState.clone();
			return result;
		}
		catch (CloneNotSupportedException bullshit) {
			throw new Error(bullshit);
		}
	}

	public long getBytePos() {
		return limitByteAddress - (localLimit - localPos);
	}

	public int available() {
		// could be more buffers, or more in this buffer, but synchronizing
		// to check this could cause us to block.
		return localLimit-localPos;
	}

	public boolean markSupported() {
		return true;
	}

	public void mark(int readlimit) {
		markedState= (BufferChainIStream) clone();
	}

	public void reset() throws IOException {
		if (markedState == null)
			throw new IllegalStateException("Must call 'mark' before 'reset'");
		curBlock= markedState.curBlock;
		localPos= markedState.localPos;
		localLimit= markedState.localLimit;
		limitByteAddress= markedState.limitByteAddress;
	}

	public int read() throws IOException {
		if (localPos == localLimit)
			if (!nextBlock())
				return -1;
		return curBlock.buffer[localPos++] & 0xFF;
	}

	public int read(byte[] buffer, int offset, int length) throws IOException {
		if (length < 0) {
			if (length == 0)
				return 0;
			else
				throw new IndexOutOfBoundsException();
		}
		if (localPos == localLimit)
			if (!nextBlock())
				return -1;
		int maxSegment= Math.min(length, localLimit-localPos);
		System.arraycopy(curBlock.buffer, localPos, buffer, offset, maxSegment);
		localPos+= maxSegment;
		return maxSegment;
	}

	public int readUnsignedByte() throws IOException {
		if (localPos == localLimit)
			if (!nextBlock())
				throw new EOFException();
		return curBlock.buffer[localPos++] & 0xFF;
	}

	public void readFully(byte[] buffer, int offset, int length) throws IOException {
		int bufferLimit= offset+length;
		while (offset < bufferLimit) {
			if (localPos == localLimit)
				if (!nextBlock())
					throw new EOFException();
			int maxSegment= Math.min(bufferLimit-offset, localLimit-localPos);
			System.arraycopy(curBlock.buffer, localPos, buffer, offset, maxSegment);
			offset+= maxSegment;
			localPos+= maxSegment;
		}
	}

	public long skip(long n) throws IOException {
		long targetAddr= getBytePos() + n;
		while (limitByteAddress < targetAddr) {
			localPos= localLimit; // skip all characters in the current buffer
			if (!nextBlock())
				return targetAddr - limitByteAddress;
		}
		localPos= localLimit - (int)(limitByteAddress - targetAddr);
		return n;
	}

	public void forceSkip(long n) throws IOException {
		if (n <= 0) {
			if (n == 0) return;
			throw new IllegalArgumentException("Parameter to forceSkip must be positive");
		}
		long targetAddr= getBytePos() + n;
		while (limitByteAddress < targetAddr) {
			localPos= localLimit; // skip all characters in the current buffer
			if (!nextBlock())
				throw new EOFException();
		}
		localPos= localLimit - (int)(limitByteAddress - targetAddr);
	}

	protected Block newBlock() {
		Block b= new Block();
		b.next= null;
		b.bufferPos= 0;
		b.buffer= new byte[BUFFER_SIZE];
		return b;
	}

	protected boolean nextBlock() throws IOException {
		synchronized (source) {
			// is more in the buffer since we looked last?
			if (curBlock.bufferPos > localLimit) {
				limitByteAddress+= curBlock.bufferPos -localLimit;
				localLimit= curBlock.bufferPos;
				return true;
			}
			// need to go to the next block
			else if (curBlock.next != null) {
				curBlock= curBlock.next;
				localPos= 0;
				localLimit= curBlock.bufferPos;
				limitByteAddress+= curBlock.bufferPos;
				return true;
			}
			else
				// either need more in this block, or a new block
				return readMore();
		}
	}

	protected boolean readMore() throws IOException {
		Block dest= curBlock;
		if (curBlock.bufferPos + MIN_READ_SIZE > curBlock.buffer.length) {
			dest= newBlock();
			curBlock.next= dest;
		}
		int result= source.read(dest.buffer, dest.bufferPos, dest.buffer.length-dest.bufferPos);
		if (result <= 0)
			return false;
		dest.bufferPos+= result;
		nextBlock();
		return true;
	}
}
