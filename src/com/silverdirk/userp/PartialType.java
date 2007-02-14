package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Set;
import java.util.HashSet;
import java.io.IOException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Partially-defined type</p>
 * <p>Description: Partial types allow a type to be defined when it has references to other types that circularly refer back to it.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class PartialType extends UserpType {
	UserpType resolvedType= null;

	public PartialType(String name) {
		this(nameToMeta(name));
	}
	public PartialType(Object[] meta) {
		super(meta);
	}
	public PartialType(String name, TypeDef def) {
		this(nameToMeta(name), def);
	}
	public PartialType(Object[] meta, TypeDef def) {
		super(meta);
		this.def= def;
	}

	void setMeta(Object[] meta) {
		super.initMeta(meta);
	}

	void setDef(TypeDef d) {
		def= d;
	}

	public UserpType makeSynonym(Object[] newMeta) {
		return new PartialType(newMeta, def);
	}

	protected int calcHash() {
		throw new UseOfTypeBeforeResolvedException(name, "calcHash (called from hashCode)");
	}

	public boolean equals(UserpType other) {
		throw new UseOfTypeBeforeResolvedException(name, "equals");
	}

	public boolean definitionEquals(UserpType other) {
		throw new UseOfTypeBeforeResolvedException(name, "definitionEquals");
	}

	public String toString() {
		return "PartialType("+name+", "+def+")";
	}

	public boolean isScalar() {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "isScalar");
		try {
			return def.isScalar();
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "isScalar");
			throw ex;
		}
	}
	public boolean hasScalarComponent() {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "hasScalarComponent");
		try {
			return def.hasScalarComponent();
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "hasScalarComponent");
			throw ex;
		}
	}
	public BigInteger getScalarRange() {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "getScalarRange");
		try {
			return def.getScalarRange();
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "getScalarRange");
			throw ex;
		}
	}
	public int getScalarBitLen() {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "getScalarBitLen");
		try {
			return def.getScalarBitLen();
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "getScalarBitLen");
			throw ex;
		}
	}

	public int getTypeRefCount() {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "getScalarBitLen");
		try {
			return def.getTypeRefCount();
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "getScalarBitLen");
			throw ex;
		}
	}

	public UserpType getTypeRef(int idx) {
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "getScalarBitLen");
		try {
			return def.getTypeRef(idx);
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "getScalarBitLen");
			throw ex;
		}
	}

	public void typeSwap(UserpType from, UserpType to) {
		if (def != null)
			def.typeSwap(from, to);
	}
	public UserpType resolve() {
		return resolve(null);
	}
	public synchronized UserpType resolve(Object typeSpace) {
		if (resolvedType == null) {
			HashSet followupSet= new HashSet(), workList= new HashSet();
			workList.add(this);
			do {
				Iterator itr= workList.iterator();
				PartialType current= (PartialType) itr.next();
				itr.remove();
				current.resolve(followupSet, workList);
			} while (workList.size() > 0);
			Iterator itr= followupSet.iterator();
			while (itr.hasNext())
				((UserpType)itr.next()).finishInit();
		}
		return resolvedType;
	}

	private void resolve(Set followupSet, Set resolveSet) {
		boolean incomplete= false;
		if (def == null)
			throw new UseOfTypeBeforeResolvedException(name, "resolve");
		try {
			resolvedType= def.resolve(this);
		}
		catch (UseOfTypeBeforeResolvedException ex) {
			ex.chainFrom(name, "resolve");
			throw ex;
		}
		for (int i=0; i<def.typeRefs.length; i++)
			if (def.typeRefs[i] instanceof PartialType && ((PartialType)def.typeRefs[i]).resolvedType == null) {
				resolveSet.add(def.typeRefs[i]);
				incomplete= true;
			}
		if (incomplete)
			followupSet.add(this);
		else
			resolvedType.finishInit();
	}

	static class NonImpl implements ReaderImpl {
		public void inspectActualType(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
		public void inspectElements(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
		public void nextElement(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
		public void endElements(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
		public int loadValue(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
		public void skipValue(UserpReader reader) throws IOException {
			throw new UseOfTypeBeforeResolvedException("?", "readerImpl");
		}
	}
}

class UseOfTypeBeforeResolvedException extends RuntimeException {
	ArrayList chain= new ArrayList();

	public UseOfTypeBeforeResolvedException(String typeName, String methodName) {
		super();
		chain.add(typeName);
		chain.add(methodName);
	}
	public String getMessage() {
		StringBuffer msg= new StringBuffer("Attempt to access the property ");
		msg.append(chain.get(1)).append(" before type ").append(chain.get(0)).append("was completely resolved;");
		for (int i=2; i<chain.size(); i+= 2)
			msg.append(" needed by ").append(chain.get(i)+"."+chain.get(i+1));
		return msg.toString();
	}
	public void chainFrom(String typeName, String methodName) {
		chain.add(methodName);
		chain.add(typeName);
	}
}
