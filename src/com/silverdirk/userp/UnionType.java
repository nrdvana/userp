package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Union</p>
 * <p>Description: This type is a unions of values from other types.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class UnionType extends UserpType {
	UnionDef def= null;
	boolean[] inlines= null;
	boolean bitpack= false;

	public static class UnionDef extends TypeDef {
		UserpType[] members;

		// TODO: find optimal value for HASH_THRESHHOLD
		static final int
			HASH_THRESHHOLD= 8; // max number of subtypes we allow before we decide to hash them
		HashMap typeIdxMap= null; // remains null unless threshhold is exceeded

		public UnionDef(UserpType[] Members) {
			if (Members.length < 2)
				throw new RuntimeException("Cannot form a union of fewer than two sub-types");
			this.members= (UserpType[]) Members.clone();

			// build hash if we have more than THRESHHOLD Member types
			if (Members.length > HASH_THRESHHOLD) {
				typeIdxMap= new HashMap();
				for (int i=0; i<Members.length; i++)
					typeIdxMap.put(Members[i].handle, new Integer(i));
			}
		}

		public int hashCode() {
			return Arrays.deepHashCode(members);
		}

		protected boolean equals(TypeDef other, Map<TypeHandle,TypeHandle> equalityMap) {
			throw new Error("Unimplemented");
//			return (other instanceof UnionDef)
//				&& Arrays.equals(members, ((UnionDef)other).Members);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer("Union([");
			for (int i=0, max=members.length-1; i <= max; i++) {
				sb.append(members[i].getName());
				if (i != max) sb.append(", ");
			}
			sb.append("])");
			return sb.toString();
		}

		public int getMemberCount() {
			return members.length;
		}

		public UserpType getMember(int idx) {
			return members[idx];
		}

		public int getMemberIdx(UserpType type) {
			// if we have a lookup table, use it
			if (typeIdxMap != null) {
				Integer result= (Integer) typeIdxMap.get(type.handle);
				if (result != null)
					return result.intValue();
			}
			// else use the boring algorithm
			for (int i=0; i<members.length; i++)
				if (members[i] == type)
					return i;
			// else fail
			return -1;
		}
	}

	public UnionType(String name) {
		this(new Symbol(name));
	}

	public UnionType(Symbol name) {
		super(name);
	}

	public UnionType init(UserpType[] Members) {
		return init(new UnionDef(Members));
	}

	public UnionType init(UnionDef def) {
		this.def= def;
		return this;
	}

	public UnionType setEncParams(boolean bitpack, boolean[] inlineMembers) {
		this.bitpack= bitpack;
		this.inlines= inlineMembers;
		return this;
	}

	public UserpType cloneAs(Symbol newName) {
		return new UnionType(newName).init(def);
	}

	public TypeDef getDefinition() {
		return def;
	}

	public boolean hasEncoderParamDefaults() {
		return inlines != null;
	}

	public boolean getEncoderParam_Bitpack() {
		return bitpack;
	}

	public boolean[] getEncoderParam_Inline() {
		return inlines;
	}

	public final int getMemberCount() {
		return def.getMemberCount();
	}

	public final UserpType getMember(int idx) {
		return def.getMember(idx);
	}

	public final int getMemberIdx(UserpType type) {
		return def.getMemberIdx(type);
	}
}
