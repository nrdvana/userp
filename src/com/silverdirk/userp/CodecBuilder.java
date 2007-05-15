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

	protected Codec addDescriptor(Codec c) {
		Codec curVal= typeMap.get(c.type.handle);
		if (curVal == null) {
			typeMap.put(c.type.handle, c);
			if (c.initStatus <= Codec.ISTAT_NEED_DESC_REFS)
				resolveRefsList.add(c);
			else if (c.initStatus != Codec.ISTAT_READY)
				resolveCodecList.add(c);
		}
		else if (curVal.equals(c))
			c= curVal;
		else
			throw new RuntimeException("Attempt to re-add type "+c.type+" with different encoding parameters");
		return c;
	}

	public Codec getCodecFor(UserpType t) {
		Codec result= getIncompleteCodecFor(t);
		finishCodecInit();
		return result;
	}

	Codec getIncompleteCodecFor(UserpType t) {
		Codec result= typeMap.get(t.handle);
		if (result != null)
			return result;
		if (!autoAdd)
			throw new RuntimeException("Foreign type encountered while automatic type inclusion was disabled: "+t);
		return addDescriptor(Codec.forType(t));
	}

	protected void finishCodecInit() {
		// can't use an iterator because new types could get added
		while (!resolveRefsList.isEmpty()) {
			Codec c= resolveRefsList.removeFirst();
			c.resolveDescriptorRefs(this);
			if (c.initStatus != Codec.ISTAT_READY)
				resolveCodecList.add(c);
		}
		if (!resolveCodecList.isEmpty()) {
			// alloc the codec objects
			for (Codec c: resolveCodecList)
				c.resolveCodec();
			// clear the list, because they're all resolved
			resolveCodecList.clear();
		}
	}
}
