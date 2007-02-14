package com.silverdirk.userp;

import java.io.IOException;
import java.util.*;
import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Userp Data Reader</p>
 * <p>Description: This class has the data-reading functionality needed for the UserpReader.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
class UserpDataReader {
	ByteSequence bseq;
	ByteSequence.IStream src;
	TypeMap codeMap;

	boolean bitpack= false;
	int bitBuffer; // The buffer contains 0..7 bits of data, right-aligned. The upper 24 bits of the buffer may contain junk
	int bitCount; // the number of valid bits in the buffer
	final byte[] tempBuffer= new byte[65]; // big enough to hold large RSA keys, since thats what I expect to be the most common use of BigInteger

	BigInteger scalar;
	long scalar64;
	boolean scalarIsLoaded;

	boolean typeTableInProgress= false;

	WarningHandler handler;

	public static final class Conditions {
		public static final int
			UNEXPECTED_TYPE= 0;
		static final int
			COND_COUNT= 1;
	}

	public static final class Actions {
		public static final int
			IGNORE= 0,
			LOG_ONCE= 1,
			LOG_EACH= 2,
			ABORT= 3;
	}

	public static final class WarningHandler {
		int[] conf= new int[Conditions.COND_COUNT];

		LinkedList warnings;
		int unexpectedTypeEventCount;
		HashSet unexpectedTypes;

		void logUnexpectedType(UserpType misfit, UserpReader r) {
			unexpectedTypeEventCount++;
			int acn= conf[Conditions.UNEXPECTED_TYPE];
			if (acn != Actions.IGNORE) {
				if (unexpectedTypes == null)
					unexpectedTypes= new HashSet();
				if (unexpectedTypes.add(misfit.handle) || acn != Actions.LOG_ONCE) {
					String msg= "At "+r.getElemPath()+": No known types are equivalent to the inline definition of "+misfit;
					if (acn == Actions.LOG_EACH)
						logMsg(msg);
					else
						throw new RuntimeException(msg);
				}
			}
		}

		void logMsg(String msg) {
			if (warnings == null)
				warnings= new LinkedList();
			warnings.add(msg);
		}
	}

	UserpDataReader(ByteSequence seq, ByteSequence.IStream src, TypeMap typeMap) {
		this.bseq= seq;
		this.src= src;
		this.codeMap= typeMap;
	}

	static class Checkpoint {
		ByteSequence seq;
		long startAddr;
		TypeMap codes;

		public Checkpoint(ByteSequence seq, long startAddr, TypeMap codes) {
			this.seq= seq;
			this.startAddr= startAddr;
			this.codes= codes;
		}
	}

	static class TypeMap {
		int chainLength;
		TypeMap base;
		HashMap typeByCode;

		protected TypeMap() {}

		TypeMap(TypeMap baseMap) {
			base= baseMap;
			typeByCode= new HashMap();
			if (base.chainLength >= 5)
				while (base.chainLength != 0) {
					typeByCode.putAll(base.typeByCode);
					base= base.base;
				}
			chainLength= base.chainLength + 1;
		}

		void redefine(int startIdx, UserpType[] newTypes) {
			for (int i=0; i<newTypes.length; i++)
				redefine(startIdx+i, newTypes[i]);
		}

		void redefine(int idx, UserpType t) {
			typeByCode.put(new Integer(idx), t);
		}

		UserpType getType(int code) {
			UserpType t= (UserpType) typeByCode.get(new Integer(code));
			return (t != null)? t : base.getType(code);
		}
	}

	static class RootTypeMap extends TypeMap {
		UserpType[] predefList;
		RootTypeMap(UserpType[] predefList) {
			chainLength= 0;
			this.predefList= predefList;
			for (int i=0; i<predefList.length; i++)
				if (predefList[i].staticProtocolTypeCode == -1)
					predefList[i].staticProtocolTypeCode= i;
				else if (predefList[i].staticProtocolTypeCode != i)
					throw new Error("typeList["+i+"] \""+predefList[i].name+"\" has already been assigned an index...");
		}

		UserpType getType(int code) {
			return (code < predefList.length)? predefList[code] : null;
		}
	}

	Checkpoint makeCheckpoint() {
		Checkpoint result= new Checkpoint(bseq, src.getSequencePos(), codeMap);
		codeMap= new TypeMap(codeMap);
		return result;
	}

	void enableBitpack(boolean enable) {
		bitpack= enable;
		if (!bitpack)
			bitCount= 0;
	}

	final BigInteger scalarAsBigInt() {
		return (scalar != null)? scalar : BigInteger.valueOf(scalar64);
	}

	final long scalarAsLong() {
		if (scalar != null) {
			if (scalar.bitLength() > 63)
				throw new LibraryLimitationException(scalar, 63);
			return scalar.longValue();
		}
		else
			return scalar64;
	}

	final long scalarAsLong(int maxBits) {
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

	final int scalarAsInt() {
		return (int) scalarAsLong(31);
	}

	final int scalarAsInt(int maxBits) {
		return (int) scalarAsLong(maxBits);
	}

	final void readVarQty() throws IOException {
//		System.out.println("readVarQty()");
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

	final void readQty(int bitLen) throws IOException {
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
				scalar64= rawRead(bitLen);
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

	final long rawRead(int bitLen) throws IOException {
		readBits(tempBuffer, 0, bitLen);
		long result= 0;
		for (int i=0; bitLen > 0; i++, bitLen-=8)
			result= (result<<8) | (tempBuffer[i]&0xFF);
		return result;
	}

	final void readBits(byte[] buffer, int offset, int bitsNeeded) throws IOException {
		if (!bitpack)
			src.forceRead(buffer, offset, (bitsNeeded+7)>>>3);
		else {
			if ((bitsNeeded & 0x7) != 0)
				buffer[offset++]= (byte) readBits8(bitsNeeded & 0x7);
			int byteCount= bitsNeeded>>>3;
			src.forceRead(buffer, offset, byteCount);
			if (bitCount != 0) {
				for (int i=offset; i<offset+byteCount; i++) {
					bitBuffer= (bitBuffer<<8) | (buffer[i]&0xFF);
					buffer[i]= (byte)(bitBuffer>>>bitCount);
				}
			}
		}
	}

	final int readBits8(int bitsNeeded) throws IOException {
		if (!bitpack)
			return src.forceRead() & 0xFF;
		else {
			if (bitsNeeded > bitCount) {
				bitBuffer= (bitBuffer<<8) | (src.forceRead()&0xFF);
				bitCount+= 8-bitsNeeded;
			}
			else
				bitCount-= bitsNeeded;
			return (bitBuffer >> bitCount) & (0xFF>>(8-bitsNeeded));
		}
	}

	final UserpType readType(UserpReader reader) throws IOException {
		readVarQty();
		int tcode= scalarAsInt();
		if (tcode == 0) {
			readTypeTable(reader);
			readVarQty();
			tcode= scalarAsInt();
			assert(tcode != 0);
		}
		return codeMap.getType(tcode-1);
	}

	final UserpType[] readTypeTable(UserpReader reader) throws IOException {
		// We do not allow type tables within type tables
		if (typeTableInProgress)
			throw new UserpProtocolException("Error in protocol stream: nested type table encountered.");
		// Find the range of codes being redefined
		readVarQty();
		int startIdx= scalarAsInt();
		readVarQty();
		int count= scalarAsInt();
		if (count == 0)
			return new UserpType[0];

		// Prevent type tables from being nested within this one
		typeTableInProgress= true;

		// Create the new types
		Object[] defaultMeta= UserpType.nameToMeta("<decode in progress>");
		PartialType[] newTypes= new PartialType[count];
		for (int i=0; i<newTypes.length; i++)
			newTypes[i]= new PartialType(defaultMeta);
		// And register them with the code-map
		codeMap.redefine(startIdx, newTypes);

		// Save a copy of the current type; we're about to abuse the fields of the reader
		UserpType prevType= reader.elemType;

		for (int i=0; i<count; i++) {
			reader.startFreshElement(ProtocolTypes.TTypedef);
			newTypes[i].setDef((TypeDef) reader.readValue());
		}
		// XXX problem here, if the metadata contains a type that is still being defined.
		// TODO: find a way to catch exceptions caused by this special case
		//  and generate the type on the fly, and resume the reading....
		// Or just disallow using a newly defined type in metadata
		// Or make a way to post-assign the metadata to an existing type object, updating things like hash code.
		for (int i=0; i<count; i++) {
			reader.startFreshElement(ProtocolTypes.TMetadata);
			newTypes[i].setMeta((TypedData[]) reader.readValue());
		}
		UserpType[] table= new UserpType[count];
		for (int i=0; i<count; i++) {
			table[i]= newTypes[i].resolve();
			codeMap.redefine(startIdx+i, table[i]);
		}

		// Restore disrupted variables in the reader
		reader.elemType= prevType;
		return table;
	}
}
