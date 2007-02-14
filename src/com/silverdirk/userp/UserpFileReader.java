package com.silverdirk.userp;

import java.io.*;
import java.math.BigInteger;
import java.util.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Userp File Reader</p>
 * <p>Description: This class reads the header for a Userp data stream, and access to the root element.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class UserpFileReader extends UserpReader {
	static final RootTypeMap rootTypes= new RootTypeMap(Protocol.VERSION_0_TYPES);
	static final long USP_MAGIC_NUM= 0x557365727000041AL;

	int version= -1;
	UserpType[] declaredTypes;
	Checkpoint fileMeta;

	public UserpFileReader(InputStream source) throws IOException {
		this(new SlidingByteSequence(source));
	}
	public UserpFileReader(SlidingByteSequence source) throws IOException {
		super(source, source.getStream(0), rootTypes);
		readHeader();
		startFreshElement(FundamentalTypes.TAny);
	}

	protected void readHeader() throws IOException {
		fileMeta= elemMeta= null;

		long magicNum= rawRead(64);
		if (magicNum != USP_MAGIC_NUM)
			throw new UserpProtocolException("Stream is not a Userp data stream.  (wrong magic number)");

		readVarQty();
		version= scalarAsInt();
		if (version != 0)
			throw new UserpProtocolException("Version of stream is newer than library can read.");

		readVarQty();
		int metaSize= scalarAsInt();
		if (metaSize > 0) {
			fileMeta= makeCheckpoint();
			src.forceSkip(metaSize);
		}

		declaredTypes= readTypeTable(this);

		readVarQty();
		metaSize= scalarAsInt();
		if (metaSize > 0) {
			elemMeta= makeCheckpoint();
			src.forceSkip(metaSize);
		}
	}

	public int getStreamVersion() throws IOException {
		if (version == -1)
			readHeader();
		return version;
	}

	public List getDeclaredTypes() throws IOException {
		if (version == -1)
			readHeader();
		return Collections.unmodifiableList(Arrays.asList(declaredTypes));
	}

	public void getStreamMetaReader() {
		throw new UnsupportedOperationException();
	}
}
