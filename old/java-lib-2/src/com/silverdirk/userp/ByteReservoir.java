package com.silverdirk.userp;

import java.io.*;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright: Copyright (c) 2004-2005</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class ByteReservoir extends OutputStream {
	static class Block {
		int pos;
		byte[] buffer;
		Block next;

		public Block(int size) {
			buffer= new byte[size];
			reset();
		}

		public void reset() {
			pos= 0;
			next= null;
		}
	}

	public static class BlockFactory {
		Block freeList;
		int blockSize;

		public BlockFactory(int blockSize) {
			this.blockSize= blockSize;
		}

		public Block alloc() {
			if (freeList == null)
				return new Block(blockSize);
			else {
				Block result= freeList;
				freeList= result.next;
				result.reset();
				return result;
			}
		}

		public void recycle(Block b) {
			b.next= freeList;
			freeList= b;
		}
	}

	Block first, cur;
	BlockFactory blockFactory;
	int blockStartAddr;

	public ByteReservoir(BlockFactory blockFactory) {
		this.blockFactory= blockFactory;
		first= cur= blockFactory.alloc();
		blockStartAddr= 0;
	}

	public int size() {
		return blockStartAddr+cur.pos;
	}

	public void write(int val) throws java.io.IOException {
		cur.buffer[cur.pos++]= (byte) val;
		if (cur.pos == cur.buffer.length)
			nextBuffer();
	}

	public void write(byte[] b, int offset, int count) throws java.io.IOException {
		int srcStop= offset+count;
		while (count > 0) {
			int n= Math.min(cur.buffer.length - cur.pos, srcStop - offset);
			System.arraycopy(b, offset, cur.buffer, cur.pos, n);
			offset+= n;
			cur.pos+= n;
			if (cur.pos == cur.buffer.length)
				nextBuffer();
		}
	}

	public void dump(OutputStream os) throws IOException {
		if (os instanceof ByteReservoir) {
			((ByteReservoir)os).addBlocks(first, cur);
			first= cur= blockFactory.alloc();
		}
		else {
			while (true) {
				if (first.pos > 0)
					os.write(first.buffer, 0, first.pos);
				if (first == cur) break;
				Block dead= first;
				first= first.next;
				blockFactory.recycle(dead);
			}
			os.write(first.buffer, 0, first.pos);
			first.pos= 0;
		}
	}

	protected void addBlocks(Block head, Block tail) {
		cur.next= head;
		cur= tail;
	}

	private void nextBuffer() {
		cur.next= blockFactory.alloc();
		cur= cur.next;
	}
}
