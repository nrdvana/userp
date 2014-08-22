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
		// all this takes place in the supposed context of reading a type table
		enc.typeTableInProgress= true;
	}

	private void prepDecoder() throws IOException {
		enc.flushToByteBoundary();
		bytes= dest.toByteArray();
		InputStream src= new ByteArrayInputStream(bytes);
		dec= new UserpDecoder(new BufferChainIStream(src), codecs);
		// all this takes place in the supposed context of reading a type table
		dec.typeTableInProgress= true;
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
		Codec ci0_d= Codec.deserialize(r);
		ci0_d.resolveDescriptorRefs(r.src.codeMap);
		ci0_d.resolveCodec();
		assertEquals(ci0.getClass(), ci0_d.getClass());
		assertEquals(ci0.type, ci0_d.type);
	}

	public void testEnum() throws Exception {
		List<UserpType> types= new ArrayList<UserpType>(5);
		types.add(new EnumType("i0").init(new Object[] {"Red", "Green", "Blue", "White", "Black"}));

		CodecBuilder cb= new CodecBuilder(true);
		List<Codec> codecs= new ArrayList<Codec>(types.size());
		for (UserpType t: types)
			codecs.add(cb.getCodecFor(t));

		prepEncoder(new Codec[] {cb.getCodecFor(Userp.TBool), cb.getCodecFor(Userp.TFloat32), cb.getCodecFor(Userp.TByteArray)});
		for (Codec c: codecs) {
			UserpWriter w= new UserpWriter(enc, Codec.CTypeSpec);
			c.serialize(w);
			w= new UserpWriter(enc, Codec.CEnumSpec);
			((EnumCodec)c).serializeSpec(w);
		}

		prepDecoder();
		for (int i=0; i<codecs.size(); i++) {
			UserpReader r= new UserpReader(dec, Codec.CTypeSpec);
			Codec c_d= Codec.deserialize(r);
			c_d.resolveDescriptorRefs(r.src.codeMap);
			c_d.resolveCodec();
			r= new UserpReader(dec, Codec.CEnumSpec);
			((EnumCodec)c_d).deserializeSpec(r);

			assertEquals("codec "+i, c_d.type, types.get(i));
			assertEquals("codec "+i, c_d, codecs.get(i)); // codec's "equals" is not as deep as it should be
			assertClassEquals("codec "+i, c_d.impl, codecs.get(i).impl);
			assertClassEquals("codec "+i, c_d.implOverride, codecs.get(i).implOverride);
		}
	}

	public void testUnion() throws Exception {
		List<UserpType> types= new ArrayList<UserpType>(5);
		types.add(new UnionType("i0").init(new UserpType[] {Userp.TBool, Userp.TFloat32, Userp.TByteArray}));

		CodecBuilder cb= new CodecBuilder(true);
		List<Codec> codecs= new ArrayList<Codec>(types.size());
		for (UserpType t: types)
			codecs.add(cb.getCodecFor(t));

		prepEncoder(new Codec[] {cb.getCodecFor(Userp.TBool), cb.getCodecFor(Userp.TFloat32), cb.getCodecFor(Userp.TByteArray)});
		for (Codec c: codecs) {
			UserpWriter w= new UserpWriter(enc, Codec.CTypeSpec);
			c.serialize(w);
		}

		prepDecoder();
		for (int i=0; i<codecs.size(); i++) {
			UserpReader r= new UserpReader(dec, Codec.CTypeSpec);
			Codec c_d= Codec.deserialize(r);
			c_d.resolveDescriptorRefs(r.src.codeMap);
			c_d.resolveCodec();
			assertEquals("codec "+i, c_d.type, types.get(i));
			assertEquals("codec "+i, c_d, codecs.get(i)); // codec's "equals" is not as deep as it should be
			assertClassEquals("codec "+i, c_d.impl, codecs.get(i).impl);
			assertClassEquals("codec "+i, c_d.implOverride, codecs.get(i).implOverride);
		}
	}

	public void testArray() throws Exception {
		List<ArrayType> types= new ArrayList<ArrayType>(5);
		types.add(new ArrayType("a0").init(Userp.TBool, 12).setEncParam(TupleCoding.PACK));
		types.add(new ArrayType("a1").init(Userp.TBool, 0).setEncParam(TupleCoding.BITPACK));
//		types.add(new ArrayType("a2").init(Userp.TBool, 12).setEncParam(TupleCoding.INDEFINITE));
//		types.add(new ArrayType("a3").init(Userp.TBool, 12).setEncParam(TupleCoding.INDEX));
		types.add(new ArrayType("a4").init(Userp.TBool, ArrayType.VARIABLE_LEN).setEncParam(TupleCoding.PACK));

		CodecBuilder cb= new CodecBuilder(true);
		List<Codec> codecs= new ArrayList<Codec>(types.size());
		for (ArrayType t: types)
			codecs.add(cb.getCodecFor(t));

		prepEncoder(new Codec[] {cb.getCodecFor(Userp.TBool)});
		for (Codec c: codecs) {
			UserpWriter w= new UserpWriter(enc, Codec.CTypeSpec);
			c.serialize(w);
		}

		prepDecoder();
		for (int i=0; i<codecs.size(); i++) {
			UserpReader r= new UserpReader(dec, Codec.CTypeSpec);
			Codec c_d= Codec.deserialize(r);
			c_d.resolveDescriptorRefs(r.src.codeMap);
			c_d.resolveCodec();
			assertEquals("codec "+i, c_d.type, types.get(i));
			assertEquals("codec "+i, c_d, codecs.get(i)); // codec's "equals" is not as deep as it should be
			assertClassEquals("codec "+i, c_d.impl, codecs.get(i).impl);
			assertClassEquals("codec "+i, c_d.implOverride, codecs.get(i).implOverride);
		}
	}

	public void testRecord() throws Exception {
		Field[] fieldSet1= new Field[] { new Field("Foo", Userp.TInt32), new Field("Bar", Userp.TFloat32) };
		Field[] fieldSet2= new Field[] { new Field("A", Userp.TInt16), new Field("B", Userp.TStrUTF8), new Field("C", Userp.TStrUTF8), new Field("D", Userp.TStrUTF8) };
		Field[] fieldSet3= new Field[] { new Field(Symbol.NIL, Userp.TNull) };

		List<RecordType> types= new ArrayList<RecordType>(5);
		types.add(new RecordType("r0").init(fieldSet1).setEncParam(TupleCoding.PACK));
		types.add(new RecordType("r1").init(fieldSet2).setEncParam(TupleCoding.BITPACK));
		types.add(new RecordType("r4").init(fieldSet3).setEncParam(TupleCoding.PACK));

		CodecBuilder cb= new CodecBuilder(true);
		List<Codec> codecs= new ArrayList<Codec>(types.size());
		for (RecordType t: types)
			codecs.add(cb.getCodecFor(t));
		prepEncoder(new Codec[] {cb.getCodecFor(Userp.TNull), cb.getCodecFor(Userp.TInt32), cb.getCodecFor(Userp.TFloat32), cb.getCodecFor(Userp.TInt16), cb.getCodecFor(Userp.TStrUTF8)});
		for (Codec c: codecs) {
			UserpWriter w= new UserpWriter(enc, Codec.CTypeSpec);
			c.serialize(w);
		}
		prepDecoder();
		for (int i=0; i<codecs.size(); i++) {
			UserpReader r= new UserpReader(dec, Codec.CTypeSpec);
			Codec c_d= Codec.deserialize(r);
			c_d.resolveDescriptorRefs(r.src.codeMap);
			c_d.resolveCodec();
			assertEquals("codec "+i, c_d.type, types.get(i));
			assertEquals("codec "+i, c_d, codecs.get(i)); // codec's "equals" is not as deep as it should be
			assertClassEquals("codec "+i, c_d.impl, codecs.get(i).impl);
			assertClassEquals("codec "+i, c_d.implOverride, codecs.get(i).implOverride);
		}
	}

	private void assertClassEquals(String msg, Object a, Object b) {
		if (a == b)
			return;
		if (a == null)
			assertEquals(msg, b.getClass(), null); // cheap way of getting an error message
		else if (b == null)
			assertEquals(msg, a.getClass(), null); // cheap way of getting an error message
		else
			assertEquals(b.getClass(), a.getClass());
	}
}
