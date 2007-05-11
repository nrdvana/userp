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
public class _TestCodecSerialization extends TestCase {
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

	public void testInteger() throws Exception {
		IntegerType i0= new IntegerType("i0");
		Codec ci0= new CodecBuilder(true).getCodecFor(i0);
		prepEncoder(new Codec[0]);
		UserpWriter w= new UserpWriter(enc, Codec.CTypeSpec);
		ci0.serialize(w);
		prepDecoder();
		UserpReader r= new UserpReader(dec, Codec.CTypeSpec);
//		Codec ci0_d= new CodecDescriptor(r, decodeParams);
//		assertEquals(ci0.getClass(), ci0_d.getClass());
//		assertEquals(ci0.getType(), ci0_d.getType());
	}

	public void testEnum() throws Exception {
		throw new Error("Unimplemented");
	}

	public void testUnion() throws Exception {
		throw new Error("Unimplemented");
	}

	public void testArray() throws Exception {
		throw new Error("Unimplemented");
	}

	public void testRecord() throws Exception {
		throw new Error("Unimplemented");
	}
}
