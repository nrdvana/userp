package com.silverdirk.userp;

import java.util.*;
import java.io.*;
import java.math.BigInteger;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class UserpEncoder {
	OutputStream dest;
	Map<UserpType.TypeHandle, Codec> typeMap;

	boolean bitpack= false;
	int bitBuffer= 0; // The buffer contains 0..7 bits of data, right-aligned. The upper 24 bits of the buffer may contain junk
	int bitCount= 0;  // the number of valid bits in the buffer

	boolean scalarTransform= false;
	long scalarOffset= 0;
	BigInteger scalarOffset_bi= null;
	int scalarBits= 0;

	boolean typeTableInProgress= false;

	byte[] tempBuffer= new byte[65];

	public void enableBitpack(boolean enable) throws IOException {
		bitpack= enable;
		if (!bitpack)
			flushToByteBoundary();
	}

	public UserpEncoder(OutputStream dest, Codec[] predefCodecs) {
		this(dest, predefCodecs, new HashMap<UserpType.TypeHandle, CodecOverride>());
	}
	public UserpEncoder(OutputStream dest, Codec[] predefCodecs, Map<UserpType.TypeHandle, CodecOverride> userCodecs) {
		this.dest= dest;
		this.typeMap= new HashMap<UserpType.TypeHandle,Codec>();
		for (Codec c: predefCodecs)
			typeMap.put(c.type.handle, c);
//		this.userCodecs= userCodecs;
	}

	public UserpEncoder(UserpEncoder cloneSource, OutputStream out) {
		this.dest= out;
		typeMap= cloneSource.typeMap;
//		userCodecs= cloneSource.userCodecs;
		bitpack= bitpack;
	}

	public void applyScalarTransform(long offset, int bitLen) {
		if (!scalarTransform) {
			scalarBits= bitLen;
			scalarTransform= true;
			scalarOffset= offset;
			scalarOffset_bi= null;
		}
		else
			scalarOffset+= offset;
	}

	public void applyScalarTransform(BigInteger offset, int bitLen) {
		if (!scalarTransform) {
			scalarBits= bitLen;
			scalarTransform= true;
			scalarOffset= 0;
			scalarOffset_bi= offset;
		}
		else
			scalarOffset_bi= (scalarOffset_bi == null)? offset : scalarOffset_bi.add(offset);
	}

	public void flush() throws IOException {
		dest.flush();
	}

	public final void writeVarQty(long val) throws IOException {
		writeQty(val, -1);
	}

	public final void writeQty(long val, int bitLen) throws IOException {
		if (scalarTransform) {
			if (scalarBits > 63 || scalarBits == -1) {
				writeQty(BigInteger.valueOf(val), bitLen);
				return;
			}
			bitLen= scalarBits;
			if (scalarOffset_bi != null)
				throw new Error("Bug");
			val+= scalarOffset;
			scalarTransform= false;
		}
		if (bitLen == -1)
			intern_writeVarQty(val);
		else
			writeBits(val, bitLen);
	}

	public final void writeVarQty(BigInteger val) throws IOException {
		writeQty(val, -1);
	}

	public final void writeQty(BigInteger val, int bitLen) throws IOException {
		if (scalarTransform) {
			bitLen= scalarBits;
			if (scalarOffset_bi != null)
				val= val.add(scalarOffset_bi);
			if (scalarOffset != 0)
				val= val.add(BigInteger.valueOf(scalarOffset));
			scalarTransform= false;
		}
		if (bitLen == -1)
			intern_writeVarQty(val);
		else
			intern_writeQty(val, bitLen);
	}

	protected void intern_writeVarQty(long val) throws IOException {
		assert(val >= 0);
		if (val < 0x20000000) {
			if (val < 0x2000) {
				if (val < 0x80) // 2^7, contained by the code
					writeBits8((int)val, 8);
				else // 2^13, 5 bits in the code, 1 extra byte
					writeBits(((int)val)|0x8000, 16);
			}
			else if (val < 0x200000) // 2^21, 5 bits in the code, 2 extra bytes
				writeBits(((int)val)|0xA00000, 24);
			else // 2^29, 5 bits in the code, 3 extra bytes
				writeBits(val|0xC0000000, 32);
		}
		else {
			int byteLen= (Util.getBitLength(val)+7)>>3;
			int code= (0xE0 | (byteLen-4));
			writeBits8(code, 8);
			writeBits(val, byteLen<<3);
		}
	}

	protected void intern_writeVarQty(BigInteger val) throws IOException {
		assert(val.signum() >= 0);
		if (val.bitLength() < 64)
			intern_writeVarQty(val.longValue());
		else {
			int byteLen= (val.bitLength()+7)>>3;
			if (byteLen-4 < 0x1F)
				writeBits8((0xE0 | (byteLen-4)), 8);
			else {
				writeBits8(0xFF, 8);
				intern_writeVarQty(byteLen);
			}
			intern_writeQty(val, byteLen<<3);
		}
	}


	protected void intern_writeQty(BigInteger val, int bitLen) throws IOException {
		assert(val.signum() >= 0);
		assert(val.bitLength() <= bitLen);
		// this would be SO MUCH SIMPLER if BigInteger would write its bytes into a supplied buffer...
		byte[] valBytes= val.toByteArray();
		int bitsAvail= valBytes.length<<3;
		// if we need more bits than BigInteger gave us, write zeroes
		// Unrolled for speed
		while (bitLen - bitsAvail > 64) {
			writeBits(0L, 64);
			bitLen-= 64;
		}
		if (bitLen > bitsAvail) {
			writeBits(0L, bitLen - bitsAvail);
			bitLen= bitsAvail;
		}
		// BigInteger writes a blank byte if its most-significant-byte's
		//  most-significant-bit is set.  We don't want that.
		int offset= (bitsAvail - bitLen) >>> 3;
		writeBitsAndDestroyBuffer(valBytes, offset, bitLen);
	}

	public final void writeBits(long value, int count) throws IOException {
		assert(count >= 0 && count <= 64);
		int i=0;
		switch ((count+7)>>3) {
		case 8: tempBuffer[i++]= (byte) (value >>> 56);
		case 7: tempBuffer[i++]= (byte) (value >>> 48);
		case 6: tempBuffer[i++]= (byte) (value >>> 40);
		case 5: tempBuffer[i++]= (byte) (value >>> 32);
		case 4: tempBuffer[i++]= (byte) (value >>> 24);
		case 3: tempBuffer[i++]= (byte) (value >>> 16);
			tempBuffer[i++]= (byte) (value >>> 8);
			tempBuffer[i++]= (byte) value;
			writeBitsAndDestroyBuffer(tempBuffer, 0, count);
			break;
		case 2:
			writeBits8((int)value>>8, count-8);
			count= 8;
		case 1:
			writeBits8((int)value, count);
		case 0:
			break;
		default:
			throw new LibraryLimitationException();
		}
	}

	// optimized for 'int'
	// on a 64-bit processor, this function is pointless
	public final void writeBits(int value, int count) throws IOException {
		assert(count >= 0 && count <= 32);
		int i=0;
		switch ((count+7)>>3) {
		case 4: tempBuffer[i++]= (byte) (value >>> 24);
		case 3: tempBuffer[i++]= (byte) (value >>> 16);
			tempBuffer[i++]= (byte) (value >>> 8);
			tempBuffer[i++]= (byte) value;
			writeBitsAndDestroyBuffer(tempBuffer, 0, count);
			break;
		case 2:
			writeBits8(value>>8, count-8);
			count= 8;
		case 1:
			writeBits8(value, count);
		case 0:
			break;
		default:
			throw new LibraryLimitationException();
		}
	}

	public void writeBits(byte[] buffer, int byteOffset, int numBits) throws IOException {
		if (!bitpack)
			dest.write(buffer, byteOffset, (numBits+7)>>>3);
		else {
			// TODO: use tempBuffer, as many times as needed, and merge this function with writeBitsAndDestroyBuffer XXX

			// in order to do the bit shifts, we can either call "write" over
			// and over for each byte, or we can copy the bytes into a
			// temporary buffer which we alter before writing as a single chunk.
			byte[] temp= new byte[(numBits+7)>>>3];
			System.arraycopy(buffer, byteOffset, temp, 0, temp.length);
			writeBitsAndDestroyBuffer(temp, 0, numBits);
		}
	}

	public void writeBitsAndDestroyBuffer(byte[] buffer, int byteOffset, int numBits) throws IOException {
		assert(numBits >= 0);
		if (!bitpack)
			dest.write(buffer, byteOffset, (numBits+7)>>>3);
		else {
			int byteLimit= byteOffset+((numBits+7)>>>3);
			int partialByteBits= numBits&0x7;
			int i= byteOffset;
			if (partialByteBits != 0) {
				int mask= (1<<partialByteBits)-1;
				bitCount+= partialByteBits;
				bitBuffer= (bitBuffer<<partialByteBits) | (buffer[byteOffset]&mask);
				if (bitCount >= 8) {
					bitCount-= 8;
					buffer[byteOffset]= (byte)(bitBuffer>>bitCount);
				}
				else
					byteOffset++;
				i++;
			}
			if (bitCount != 0)
				for (; i<byteLimit; i++) {
					bitBuffer= (bitBuffer<<8) | (buffer[i]&0xFF);
					buffer[i]= (byte)(bitBuffer>>>bitCount);
				}
			dest.write(buffer, byteOffset, byteLimit-byteOffset);
		}
	}

	public void writeBits8(int val, int count) throws IOException {
		assert(val >= 0);
		assert(count >= 0 && count <= 8);
		if (!bitpack)
			dest.write(val);
		else {
			int mask= (1<<count)-1;
			bitBuffer= (bitBuffer<<count) | (val&mask);
			bitCount+= count;
			if (bitCount >= 8) {
				bitCount-= 8;
				dest.write(bitBuffer >>> bitCount);
			}
		}
	}

	protected void flushToByteBoundary() throws IOException {
		if (bitCount != 0) {
			dest.write(bitBuffer<<(8-bitCount));
			bitCount= 0;
			bitBuffer= 0;
		}
	}

/* Little-endian implementation...

	protected void intern_writeQty(BigInteger val, int bitLen) throws IOException {
		int neededBytes= (bitLen+7)>>>3;
		byte[] protocolBytes= (neededBytes < tempBuffer.length)? tempBuffer : new byte[neededBytes];
		byte[] valBytes= val.toByteArray();
		// We have no guarantee that the length of the bigint matches the
		// bit-length that the user wants out of it, so wonky array-copying ensues.
		int toCopy= Math.min(protocolBytes.length, valBytes.length);
		for (int src=valBytes.length-1, dest=0; dest<toCopy; src--,dest++)
			protocolBytes[dest]= valBytes[src];
		if (toCopy < protocolBytes.length)
			java.util.Arrays.fill(protocolBytes, toCopy, protocolBytes.length, (byte)0);
		writeBitsAndDestroyBuffer(protocolBytes, 0, bitLen);
	}

	public final void writeBits(long value, int count) throws IOException {
		int i=0;
		switch ((count+7)>>3) {
		case 8: tempBuffer[i++]= (byte)value; value>>>=8;
		case 7: tempBuffer[i++]= (byte)value; value>>>=8;
		case 6: tempBuffer[i++]= (byte)value; value>>>=8;
		case 5: tempBuffer[i++]= (byte)value; value>>>=8;
		case 4: tempBuffer[i++]= (byte)value; value>>>=8;
		case 3: tempBuffer[i++]= (byte)value; value>>>=8;
			tempBuffer[i++]= (byte)value; value>>>=8;
			tempBuffer[i++]= (byte)value;
			writeBitsAndDestroyBuffer(tempBuffer, 0, count);
			break;
		case 2:
		case 1:
			writeBits24((int)value, count);
		case 0:
			break;
		default:
			throw new LibraryLimitationException();
		}
	}

	// optimized for 'int'
	// on a 64-bit processor, this function is pointless
	public final void writeBits(int value, int count) throws IOException {
		int i=0;
		switch ((count+7)>>3) {
		case 4: tempBuffer[i++]= (byte)value; value>>>=8;
		case 3: tempBuffer[i++]= (byte)value; value>>>=8;
			tempBuffer[i++]= (byte)value; value>>>=8;
			tempBuffer[i++]= (byte)value;
			writeBitsAndDestroyBuffer(tempBuffer, 0, count);
			break;
		case 2:
		case 1:
			writeBits24(value, count);
		case 0:
			break;
		default:
			throw new LibraryLimitationException();
		}
	}

	public void writeBitsAndDestroyBuffer(byte[] buffer, int byteOffset, int numBits) throws IOException {
		if (!bitpack)
			dest.write(buffer, byteOffset, (numBits+7)>>>3);
		else {
			int byteLimit= byteOffset+((numBits+7)>>>3);
			if (bitCount != 0) {
				for (int i=byteOffset; i<byteLimit; i++) {
					bitBuffer|= (buffer[i]&0xFF)<<bitCount;
					buffer[i]= (byte) bitBuffer;
					bitBuffer>>= 8;
				}
			}
			int partialByteBits= numBits&0x7;
			if (partialByteBits > 0) {// we have a partial byte...
				bitCount+= partialByteBits;
				if (bitCount < 8) {// and the leftover bits didn't fill it up
					bitBuffer= buffer[byteLimit-1]&((1<<bitCount)-1); // so it becomes our new buffer
					byteLimit--; // and we don't write it
				}
				else // the leftover bits did fill it up
					bitCount-= 8; // we're writing 8 of them
			}
			dest.write(buffer, byteOffset, byteLimit-byteOffset);
		}
	}

	public void writeBits24(int val, int count) throws IOException {
		int mask= (1<<count)-1; // XXX would be better to make sure Val's upper bits are clean
		bitBuffer|= (val&mask)<<bitCount;
		bitCount+= count;
		while (bitCount >= 8) {
			dest.write(bitBuffer);
			bitCount-= 8;
			bitBuffer>>= 8;
		}
	}

	protected void flushToByteBoundary() throws IOException {
		if (bitCount != 0) {
			dest.write(bitBuffer);
			bitCount= 0;
			bitBuffer= 0;
		}
	}
*/
	public void writeType(UserpType t) throws IOException {
		writeType(typeMap.get(t.handle));
	}

	public void writeType(Codec c) throws IOException {
		if (c.encoderTypeCode <= 0)
			writeTypeTable(collectTypesWithoutCodes(c));
		writeVarQty(c.encoderTypeCode);
	}

	public void writeTypeTable(Codec[] codecs) throws IOException {
		if (typeTableInProgress)
			throw new Error("BUG: attempt to write a undefined type within a type table");
		typeTableInProgress= true;
		writeVarQty(0);
		// write the starting index.  we always choose the next available code.
		int startIdx= typeMap.size();
		writeVarQty(startIdx);
		// set the type codes for each codec
		for (int i=0,limit=codecs.length; i<limit; i++)
			codecs[i].encoderTypeCode= startIdx+i;
		// write type definitions
		UserpWriter writer= new UserpWriter(this, Codec.CTypeTableSpec);
		writer.beginTuple(codecs.length);
		for (Codec c: codecs)
			c.serialize(writer);
		writer.endTuple();
		// Enum specifications must be written after, because it could involve
		//  writing values of types that we just defined
		for (Codec c: codecs)
			if (c instanceof EnumCodec) {
				writer.reInit(Codec.CEnumSpec);
				((EnumCodec)c).serializeSpec(writer);
			}
		// Write metadata
		for (Codec c: codecs) {
			writer.reInit(MetaTreeReader.CMetadata);
			writer.write(c.type.meta);
		}
		typeTableInProgress= false;
	}

	private Codec[] collectTypesWithoutCodes(Codec root) {
		HashSet<Codec.CodecHandle> set= new HashSet<Codec.CodecHandle>();
		set.add(root.handle);

		Codec[] result= new Codec[set.size()];
		int i= 0;
		for (Codec.CodecHandle ch: set)
			result[i++]= ch.owner;
		return result;
	}
}
