package com.silverdirk.userp;

import java.util.*;
import java.io.*;
import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Low-level Userp Stream Decoder</p>
 * <p>Description: Decodes the fundamental values used by the protocol, and manages the set of active codecs</p>
 * <p>Copyright Copyright (c) 2005-2007</p>
 *
 * @author Michael Conrad
 */
public class UserpDecoder {
	BufferChainIStream src;
	TypeCodeMap codeMap;
	Map<UserpType.TypeHandle, CodecOverride> userCodecs; // a static map of user-specified codecs

	boolean bitpack= false; // whether bitpacking is enabled currently
	int bitBuffer= 0; // The buffer contains 0..7 bits of data, right-aligned. The upper 24 bits of the buffer may contain junk
	int bitCount= 0;  // the number of valid bits in the buffer
	final byte[] tempBuffer= new byte[65]; // big enough to hold large RSA keys, since thats what I expect to be the most common use of BigInteger

	BigInteger scalar= null;
	long scalar64;
	boolean qtyOverride= false; // whether the next quantity has been pre-specified

	boolean typeTableInProgress= false;

	public UserpDecoder(BufferChainIStream src, Codec[] predefCodecs) {
		this.src= src;
		codeMap= new RootTypeMap(predefCodecs);
	}

	static class TypeCodeMap {
		int chainLength;
		TypeCodeMap base;
		HashMap<Integer,Codec> typeByCode;

		protected TypeCodeMap() {}

		TypeCodeMap(TypeCodeMap baseMap) {
			base= baseMap;
			typeByCode= new HashMap();
			if (base.chainLength >= 5)
				while (base.chainLength != 0) {
					typeByCode.putAll(base.typeByCode);
					base= base.base;
				}
			chainLength= base.chainLength + 1;
		}

		void redefine(int startIdx, Codec[] newTypes) {
			for (int i=0; i<newTypes.length; i++)
				redefine(startIdx+i, newTypes[i]);
		}

		void redefine(int idx, Codec t) {
			typeByCode.put(new Integer(idx), t);
		}

		UserpType getType(int code) throws UserpProtocolException {
			return getCodec(code).type;
		}

		Codec getCodec(int code) throws UserpProtocolException {
			Codec t= typeByCode.get(new Integer(code));
			return (t != null)? t : base.getCodec(code);
		}
	}

	static class RootTypeMap extends TypeCodeMap {
		Codec[] predefList;
		RootTypeMap(Codec[] predefList) {
			chainLength= 0;
			this.predefList= predefList;
		}

		Codec getCodec(int code) throws UserpProtocolException {
			if (code < predefList.length) return predefList[code];
			throw new UserpProtocolException("Invalid type code encountered: "+code);
		}
	}

	public void enableBitpack(boolean enable) {
		bitpack= enable;
		if (!bitpack)
			bitCount= 0;
	}

	/** Force the next call to readQty or readVarQty to load this value.
	 * This is used by Union to dissect a scalar value and then give a modified
	 * value to the next codec that calls readQty/readVarQty
	 *
	 * @param val long The value to use as the next scalar value
	 */
	public final void setNextQty(long val) {
		qtyOverride= true;
		scalar= null;
		scalar64= val;
	}

	/** Force the next call to readQty or readVarQty to load this value.
	 * This is used by Union to dissect a scalar value and then give a modified
	 * value to the next codec that calls readQty/readVarQty
	 *
	 * @param val BigInteger The value to use as the next scalar value
	 */
	public final void setNextQty(BigInteger val) {
		qtyOverride= true;
		scalar= val;
	}

	/** Get the bit-length of the scalar value
	 *
	 * @return int The number of significant bits in the scalar value
	 */
	public final int getScalarBitLen() {
		return (scalar != null)? scalar.bitLength() : Util.getBitLength(scalar64);
	}

	/** Get the scalar value loaded by a previous call to readQty or readVarQty
	 *
	 * @return BigInteger
	 */
	public final BigInteger scalarAsBigInt() {
		return (scalar != null)? scalar : BigInteger.valueOf(scalar64);
	}

	/** Get the scalar value, throwing an error if the value exceeds 63 bits.
	 *
	 * @return long the scalar value
	 */
	public final long scalarAsLong() {
		if (scalar != null) {
			if (scalar.bitLength() > 63)
				throw new LibraryLimitationException(scalar, 63);
			return scalar.longValue();
		}
		else
			return scalar64;
	}

	/** Get the scalar value, throwing an error if the value has more than the requested number of bits.
	 *
	 * @param maxBits int the maximum number of bits permitted
	 * @return long the scalar value
	 */
	public final long scalarAsLong(int maxBits) {
		if (scalar != null) {
			if (scalar.bitLength() > maxBits)
				throw new LibraryLimitationException(scalar, maxBits);
			return scalar.longValue();
		}
		else {
			if ((scalar64>>maxBits) != 0)
				throw new LibraryLimitationException(new Long(scalar64), maxBits);
			return scalar64;
		}
	}

	/** Get the scalar value, throwing an error if the value has more than 31 bits.
	 *
	 * @return int the scalar value
	 */
	public final int scalarAsInt() {
		return (int) scalarAsLong(31);
	}

	/** Get the scalar value, throwing an error if the value has more than the requested number of bits.
	 *
	 * @param maxBits int the maximum number of bits permitted
	 * @return int the scalar value
	 */
	public final int scalarAsInt(int maxBits) {
		return (int) scalarAsLong(maxBits);
	}

	/** Read a variable-length quantity.
	 * <p>This function performs one or more read operations to determine the
	 * length of the quantity and load it.  The reads are always a whole number
	 * of bytes (multiple of 8 bits) though these might cross byte boundaries
	 * if the stream was previously put into bitpack mode.
	 *
	 * <p>The protocol places no upper limit on the size of a quantity, but this
	 * implementation bears the limits of available memory, address space,
	 * and Java's limits on size of arrays inherent in BigInteger.
	 *
	 * <p>The value read is stored in a field of this object.  To get the value,
	 * call either scalarAsBigInt() or scalarAsLong().  You can inspect the
	 * number of significant bits using getScalarBitLen();
	 *
	 * @throws IOException if the underlying stream encounters an error
	 */
	public final void readVarQty() throws IOException {
		if (qtyOverride) {
			qtyOverride= false;
			return;
		}
		scalar= null;
		int firstByte= readBits8(8);
		int additionalBits;
		if (firstByte < 0x80)
			scalar64= firstByte;
		else {
			int flag= (firstByte >> 5) & 3;
			int low5bits= firstByte & 0x1F;
			if (flag < 3) {
				additionalBits= (flag+1) << 3;
				readQty(additionalBits);
				scalar64|= low5bits << additionalBits;
			}
			else {
				if (low5bits == 0x1F) {
					readVarQty();
					additionalBits= scalarAsInt(28)<<3;
				}
				else
					additionalBits= (low5bits+4)<<3;
				readQty(additionalBits);
			}
		}
	}

	/** Read an n-bit quantity.
	 * <p>This function reads the specified number of bits from the stream.  If
	 * the stream is in bitpack mode, these bits will be read in big-endian
	 * order starting with the most-significant-bit of the current byte. If the
	 * stream is NOT in bitpack mode, these bits will be read from a whole
	 * number of bytes in bigendian order, where the least-significant-bit is
	 * aligned to the least-significant-bit of the final byte.
	 *
	 * <p>To restate: in normal byte mode, the bits are aligned exactly as they
	 * would be on a big-endian architecture; in bitpack mode, those same bits
	 * are shifted left to fill the MSB and leave unused bits in the LSB.
	 *
	 * <p>If BitLen is -1, readVarQty() is called instead.
	 *
	 * <p>The value read is stored in a field of this object.  To get the value,
	 * call either scalarAsBigInt() or scalarAsLong().  You can inspect the
	 * number of significant bits using getScalarBitLen();
	 *
	 * @param bitLen int The number of bits to read, or -1 to read a VarQty
	 * @throws IOException if the underlying stream encounters an error
	 */
	public final void readQty(int bitLen) throws IOException {
		if (qtyOverride) {
			qtyOverride= false;
			return;
		}
		scalar= null;
		if (bitLen <= 0) {
			if (bitLen == 0)
				scalar64= 0;
			else
				// on the assumption that someType.getBitLength() == TypeDef.VAR_LENGTH == -1
				readVarQty();
		}
		else if (bitLen < 64) {
			if (bitLen <= 8)
				scalar64= readBits8(bitLen);
			else
				scalar64= readBits(bitLen);
		}
		else {
			int byteLen= (bitLen+7)>>>3;
			byte[] data= byteLen < tempBuffer.length? tempBuffer : new byte[byteLen + 1];
			int ofs= data.length - byteLen;
			java.util.Arrays.fill(data, 0, ofs, (byte)0);
			readBits(data, ofs, bitLen);
			int firstNonzero= ofs;
			while (firstNonzero<data.length && data[firstNonzero] == 0)
				firstNonzero++;
			int valLength= data.length - firstNonzero;
			if (valLength < 8 || (valLength==8 && data[firstNonzero] > 0)) {
				scalar64= data[firstNonzero++]&0xFF;
				while (firstNonzero < data.length)
					scalar64= (scalar64<<8) | (data[firstNonzero++]&0xFF);
			}
			else
				scalar= new BigInteger(data);
		}
	}

	/** Read a number of bits and return it as a 'long'.
	 *
	 * @param bitsNeeded int the number of bits to read.
	 * @return long The bits read, in big-endian order right-aligned on the least-significant-bit
	 * @throws IOException if there is an error in the underlying stream
	 */
	final long readBits(int bitsNeeded) throws IOException {
		readBits(tempBuffer, 0, bitsNeeded);
		long result= 0;
		for (int i=0; bitsNeeded > 0; i++, bitsNeeded-=8)
			result= (result<<8) | (tempBuffer[i]&0xFF);
		return result;
	}

	/** Read a number of bits into the specified buffer, right-aligned.
	 * buffer[offset] will contain the first bits, and will be only partially
	 * filled if bitsNeeded is not a multiple of 8.
	 *
	 * @param buffer byte[] the receiving buffer
	 * @param offset int the byte-offset within the buffer where the first bits should be stored
	 * @param bitsNeeded int the number of bits to read into the buffer
	 * @throws IOException if there is an error in the underlying stream
	 */
	final void readBits(byte[] buffer, int offset, int bitsNeeded) throws IOException {
		if (!bitpack) {
			src.readFully(buffer, offset, (bitsNeeded+7)>>>3);
			if ((bitsNeeded&7) > 0)
				buffer[offset]&= (1<<(bitsNeeded&7))-1; // clear unused bits
		}
		else {
			if ((bitsNeeded & 0x7) != 0)
				buffer[offset++]= (byte) readBits8(bitsNeeded & 0x7);
			int byteCount= bitsNeeded>>>3;
			src.readFully(buffer, offset, byteCount);
			if (bitCount != 0) {
				for (int i=offset, limit=offset+byteCount; i<limit; i++) {
					bitBuffer= (bitBuffer<<8) | (buffer[i]&0xFF);
					buffer[i]= (byte)(bitBuffer>>>bitCount);
				}
			}
		}
	}

	final void skipBits(int bitsToSkip) throws IOException {
		if (!bitpack)
			src.skip((bitsToSkip+7)>>3);
		else {
			bitsToSkip-= bitCount;
			int byteSkip= bitsToSkip>>>3;
			src.skip(byteSkip);
			bitCount= bitsToSkip & 0x7;
			if (bitCount != 0)
				bitBuffer= src.readUnsignedByte();
		}
	}

	/** Read up to 8 bits from the underlying stream.
	 * If bitpacking is not enabled, this function reads an entire byte.
	 * The upper bits of the returned int will be clear.
	 *
	 * @param bitsNeeded int The number of bits to read, limited to 8
	 * @return int An integer containing those bits
	 * @throws IOException if there is an error in the underlying stream
	 */
	final int readBits8(int bitsNeeded) throws IOException {
		if (!bitpack)
			return src.readUnsignedByte() & ((1<<bitsNeeded)-1);
		else {
			if (bitsNeeded > bitCount) {
				bitBuffer= (bitBuffer<<8) | src.readUnsignedByte();
				bitCount+= 8-bitsNeeded;
			}
			else
				bitCount-= bitsNeeded;
			return (bitBuffer >> bitCount) & ((1<<bitsNeeded)-1);
		}
	}

/* Little-endian implementation...

	public final void readQty(int bitLen) throws IOException {
		scalar= null;
		if (bitLen <= 0) {
			if (bitLen == 0)
				scalar64= 0;
			else
				// on the assumption that someType.getBitLength() == TypeDef.VAR_LENGTH == -1
				readVarQty();
		}
		else if (bitLen < 64) {
			if (bitLen <= 8)
				scalar64= readBits8(bitLen);
			else
				scalar64= readBits(bitLen);
		}
		else {
			int bytesNeeded= 1+(bitLen+7)>>3; // extra byte to prevent bigint sign-extend
			byte[] buffer= (bytesNeeded < tempBuffer.length)? tempBuffer : new byte[bytesNeeded];
			int offset= buffer.length-(bytesNeeded-1);
			readBits(buffer, offset, bitLen);
			int valLimit= offset;
			while (valLimit<buffer.length && buffer[valLimit] != 0)
				valLimit++;
			if (valLimit-offset < 8 || (valLimit-offset==8 && buffer[valLimit-1] > 0)) {
				scalar64= 0;
				int val;
				for (int shift=0,i=offset; i<valLimit; i++,shift+=8)
					scalar64|= (buffer[offset++]&0xFF)<<shift;
			}
			else {
				java.util.Arrays.fill(buffer, 0, offset, (byte)0);
				// now play the big-endian game
				for (int i=offset, j=buffer.length-1; i<j; i++,j--) {
					// yeah I know about the XOR trick... but in java this is probably faster
					byte x= buffer[i];
					buffer[i]= buffer[j];
					buffer[j]= x;
				}
				scalar= new BigInteger(buffer);
			}
		}
	}

	public final long readBits(int bitsNeeded) throws IOException {
		if (bitsNeeded<=8)
			return readBits8(bitsNeeded);
		readBits(tempBuffer, 0, bitsNeeded);
		long result= 0;
		switch ((bitsNeeded+7)>>>3) {
		case 8: result|= (tempBuffer[7]&0xFF)<<56;
		case 7: result|= (tempBuffer[6]&0xFF)<<48;
		case 6: result|= (tempBuffer[5]&0xFF)<<40;
		case 5: result|= (tempBuffer[4]&0xFF)<<32;
		case 4: result|= (tempBuffer[3]&0xFF)<<24;
		case 3: result|= (tempBuffer[2]&0xFF)<<16;
		case 2: result|= (tempBuffer[1]&0xFF)<<8;
		case 1: result|= (tempBuffer[0]&0xFF);
		}
		return result;
	}

	public final void readBits(byte[] buffer, int offset, int bitsNeeded) throws IOException {
		if (!bitpack)
			src.readFully(buffer, offset, (bitsNeeded+7)>>>3);
		else {
			int byteCount= bitsNeeded>>>3;
			src.readFully(buffer, offset, byteCount);
			if (bitCount != 0) {
				for (int i=offset, limit=offset+byteCount; i<limit; i++) {
					bitBuffer|= (buffer[i]&0xFF) << bitCount;
					buffer[i]= (byte)bitBuffer;
					bitBuffer>>= 8;
				}
			}
			if ((bitsNeeded & 0x7) != 0)
				buffer[offset+byteCount]= (byte) readBits8(bitsNeeded & 0x7);
		}
	}

	public final int readBits8(int bitsNeeded) throws IOException {
		if (!bitpack)
			return src.readUnsignedByte();
		else {
			if (bitsNeeded > bitCount) {
				bitBuffer|= src.readUnsignedByte()<<bitCount;
				bitCount+= 8-bitsNeeded;
			}
			else
				bitCount-= bitsNeeded;
			int result= bitBuffer&((1<<bitsNeeded)-1);
			bitBuffer>>= bitsNeeded;
			return result;
		}
	}
*/
	public final Codec readType() throws IOException {
		readVarQty();
		int tcode= scalarAsInt();
		if (!typeTableInProgress) {
			if (tcode-- == 0) {
				readTypeTable();
				readVarQty();
				tcode= scalarAsInt();
			}
		}
		return codeMap.getCodec(tcode);
	}

	public final Codec[] readTypeTable() throws IOException {
		throw new Error("Unimplemented");
		// Find the range of codes being redefined
//		readVarQty();
//		int startIdx= scalarAsInt();
//		readVarQty();
//		int count= scalarAsInt();
//		if (count == 0)
//			return new UserpType[0];
//
//		// Prevent type tables from being nested within this one
//		typeTableInProgress= true;
//
//		// Create the new types
//		UserpType[] newTypes= new UserpType[count];
//		int[] classCodes= new int[count];
//		enableBitpack(true);
//		for (int i=0; i<newTypes.length; i++)
//			classCodes[i]= readBits8(2);
//		enableBitpack(false);
//		for (int i=0; i<newTypes.length; i++) {
//			readVarQty();
//			byte[] nameBytes= new byte[scalarAsInt()];
//			src.readFully(nameBytes);
//			newTypes[i]= typeFromClassCode(classCodes[i], new Symbol(nameBytes));
//		}
//		// And register them with the code-map
//		codeMap.redefine(startIdx, newTypes);
//
//		// Save a copy of the current type; we're about to abuse the fields of the reader
//		UserpType prevType= reader.elemType;
//
//		for (int i=0; i<count; i++) {
//			reader.startFreshElement(ProtocolTypes.TTypedef);
//			newTypes[i].setDef((TypeDef) reader.readValue());
//		}
//		// XXX problem here, if the metadata contains a type that is still being defined.
//		// TODO: find a way to catch exceptions caused by this special case
//		//  and generate the type on the fly, and resume the reading....
//		// Or just disallow using a newly defined type in metadata
//		// Or make a way to post-assign the metadata to an existing type object, updating things like hash code.
//		for (int i=0; i<count; i++) {
//			reader.startFreshElement(ProtocolTypes.TMetadata);
//			newTypes[i].setMeta((TypedData[]) reader.readValue());
//		}
//		UserpType[] table= new UserpType[count];
//		for (int i=0; i<count; i++) {
//			table[i]= newTypes[i].resolve();
//			codeMap.redefine(startIdx+i, table[i]);
//		}
//
//		// Restore disrupted variables in the reader
//		reader.elemType= prevType;
//		typeTableInProgress= false;
	}
}
