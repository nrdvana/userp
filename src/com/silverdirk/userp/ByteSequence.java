package com.silverdirk.userp;

import java.util.*;
import java.io.*;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public interface ByteSequence {
	public Object clone();
	public void clear();
	public boolean isEmpty();
	public long getLength();
	public ByteSequence subSequence(long from, long to);
	public void append(byte[] buffer, short from, short to);
	public void discard(long count);
	public IStream getStream(long fromIndex);

	public abstract class IStream extends InputStream {
		public abstract long getSequencePos();
		public abstract int available();
		public abstract boolean markSupported();
		public abstract void mark(int readlimit);
		public abstract void reset() throws IOException;
		public abstract int read() throws IOException;
		public abstract int read(byte[] buffer, int offset, int length) throws IOException;
		public abstract byte forceRead() throws IOException;
		public abstract void forceRead(byte[] buffer, int offset, int length) throws IOException;
		public abstract long skip(long n) throws IOException;
		public abstract void forceSkip(long n) throws IOException;
		public abstract void seek(long address) throws IOException;
	}
}
