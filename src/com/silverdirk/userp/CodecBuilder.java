package com.silverdirk.userp;

import java.util.*;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class CodecBuilder {
	ThreadLocal<Boolean> recursionInProgress= new ThreadLocal<Boolean>();

	protected HashMap<UserpType.TypeHandle,Codec> typeMap;
	protected LinkedList<Codec> resolveRefsList;
	protected LinkedList<Codec> resolveCodecList;
	protected boolean autoAdd;

	public CodecBuilder(boolean autoAddTypes) {
		typeMap= new HashMap<UserpType.TypeHandle,Codec>();
		resolveRefsList= new LinkedList<Codec>();
		resolveCodecList= new LinkedList<Codec>();
		autoAdd= autoAddTypes;
	}

	public void addType(UserpType t) {
		addDescriptor(Codec.forType(t));
	}

	public void addType(UnionType t, boolean bitpack, boolean[] memberInlined) {
		addDescriptor(Codec.forType(t, bitpack, memberInlined));
	}

	public void addType(TupleType t, TupleCoding encMode) {
		addDescriptor(Codec.forType(t, encMode));
	}

	protected Codec addDescriptor(Codec cd) {
		Codec curVal= typeMap.get(cd.type.handle);
		if (curVal == null) {
			typeMap.put(cd.type.handle, cd);
			if (cd.initStatus <= Codec.ISTAT_NEED_DESC_REFS)
				resolveRefsList.add(cd);
			else if (cd.initStatus != Codec.ISTAT_READY)
				resolveCodecList.add(cd);
		}
		else if (curVal.equals(cd))
			cd= curVal;
		else
			throw new RuntimeException("Attempt to re-add type "+cd.type+" with different encoding parameters");
		return cd;
	}

	public Codec getCodecFor(UserpType t) {
		Codec result= typeMap.get(t.handle);
		if (result == null) {
			if (!autoAdd)
				throw new RuntimeException("Foreign type encountered while automatic type inclusion was disabled: "+t);
			result= addDescriptor(Codec.forType(t));
		}
		finishCodecInit();
		return result;
	}

	protected void finishCodecInit() {
		// can't use an iterator because new types could get added
		while (!resolveRefsList.isEmpty()) {
			Codec cd= resolveRefsList.removeFirst();
			cd.resolveDescriptorRefs(this);
			if (cd.initStatus != Codec.ISTAT_READY)
				resolveCodecList.add(cd);
		}
		if (!resolveCodecList.isEmpty()) {
			// alloc the codec objects
			for (Codec cd: resolveCodecList)
				cd.resolveCodec();
			// clear the list, because they're all resolved
			resolveCodecList.clear();
		}
	}
}
