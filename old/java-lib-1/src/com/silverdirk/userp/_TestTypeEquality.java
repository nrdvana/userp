package com.silverdirk.userp;

import junit.framework.*;
import com.silverdirk.userp.RecordType.Field;
import com.silverdirk.userp.EnumType.Range;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: UserpType.equals() test cases</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _TestTypeEquality extends TestCase {
	public _TestTypeEquality() {
	}

	public void testStandaloneTypes() {
		IntegerType it1= new IntegerType("it1");
		IntegerType it2= new IntegerType("it2");
		IntegerType it3= new IntegerType("it1");

		assertTrue(it1.equals((Object)it1));
		assertTrue(it1.equals(it1));
		assertTrue(it1.definitionEquals(it1));

		assertFalse(it1.equals((Object)it2));
		assertFalse(it1.equals(it2));
		assertTrue(it1.definitionEquals(it2));
		assertFalse(it2.equals((Object)it1));
		assertFalse(it2.equals(it1));
		assertTrue(it2.definitionEquals(it1));

		assertTrue(it1.equals((Object)it3));
		assertTrue(it1.equals(it3));
		assertTrue(it1.definitionEquals(it3));
		assertTrue(it3.equals((Object)it1));
		assertTrue(it3.equals(it1));
		assertTrue(it3.definitionEquals(it1));
	}

	public void testShallowNonrecursiveTypes() {
		EnumType et1= new EnumType("et1").init(new Object[] {new Range(Userp.TInt, 0, 1000)});
		EnumType et2= new EnumType("et2").init(new Object[] {new Range(Userp.TInt, 0, 1000)});
		EnumType et3= new EnumType("et1").init(new Object[] {new Range(Userp.TInt, 0, 1000)});
		EnumType et4= new EnumType("et1").init(new Object[] {new Range(Userp.TInt, 0, 50000)});
		EnumType et5= new EnumType("et1").init(new Object[] {new Range(Userp.TInt64, 0, 1000)});

		assertFalse(et1.equals((Object)et2));
		assertFalse(et1.equals(et2));
		assertTrue(et1.definitionEquals(et2));
		assertTrue(et2.definitionEquals(et1));

		assertTrue(et1.equals((Object)et3));
		assertTrue(et1.equals(et3));
		assertTrue(et1.definitionEquals(et3));
		assertTrue(et3.definitionEquals(et1));

		assertFalse(et1.equals((Object)et4));
		assertFalse(et1.equals(et4));
		assertFalse(et1.definitionEquals(et4));
		assertFalse(et4.definitionEquals(et1));

		assertFalse(et1.equals((Object)et5));
		assertFalse(et1.equals(et5));
		assertFalse(et1.definitionEquals(et5));
		assertFalse(et5.definitionEquals(et1));
	}

	private RecordType createSmallTypeDAG() {
		EnumType e1= new EnumType("e1");
		EnumType e2= new EnumType("e2").init(new Object[] { "foo", "bar", "baz" });
		UnionType u1= new UnionType("u1");
		ArrayType a1= new ArrayType("a1");
		RecordType r1= new RecordType("r1");
		e1.init(new Object[] { new TypedData(e2, 0), new Range(e2, 1, 2)});
		a1.init(u1, 15);
		u1.init(new UserpType[] {e1, Userp.TInt, Userp.TBool, Userp.TNull, Userp.TByteArray});
		r1.init(new RecordType.RecordDef(new Field[] {new Field("a", e1), new Field("b", u1), new Field("c", a1)}));
		return r1;
	}

	public void testNonrecursiveTypes() {
		RecordType t1= createSmallTypeDAG();
		RecordType t2= createSmallTypeDAG();
		assertTrue(t1.equals(t2));
		assertTrue(t2.equals(t1));

		RecordType t3= createSmallTypeDAG();
		t3.def.fieldNames[2]= new Symbol("x");
		assertFalse(t1.equals(t3));
		assertFalse(t3.equals(t1));

		RecordType t4= createSmallTypeDAG();
		((UnionType)t4.def.fieldTypes[1]).def.members[2]= Userp.TAny;
		assertFalse(t1.equals(t4));
		assertFalse(t4.equals(t1));

		RecordType t5= createSmallTypeDAG();
		((TypedData)((EnumType)t5.def.fieldTypes[0]).def.spec[0]).data= 1;
		assertFalse(t1.equals(t5));
		assertFalse(t5.equals(t1));

		RecordType t6= createSmallTypeDAG();
		((EnumType)((TypedData)((EnumType)t6.def.fieldTypes[0]).def.spec[0]).dataType).def.spec[2]= new Symbol("somethingelse");
		assertFalse(t1.equals(t6));
		assertFalse(t6.equals(t1));
	}

	private UserpType createLargeTypeDAG(UserpType leafType) {
		UserpType[] t= new UserpType[25];
		t[24]= leafType;
		for (int i=t.length-2; i>=0; i--)
			t[i]= new ArrayType("t_"+i).init(t[i+1], 2);
		return t[0];
	}

	public void testDeepNonrecursiveTypes() {
		UserpType leaf1= createSmallTypeDAG();
		UserpType leaf2= createSmallTypeDAG();
		RecordType leaf3= createSmallTypeDAG();
		((EnumType)((TypedData)((EnumType)leaf3.def.fieldTypes[0]).def.spec[0]).dataType).def.spec[2]= new Symbol("somethingelse");

		UserpType t1= createLargeTypeDAG(leaf1);
		UserpType t2= createLargeTypeDAG(leaf2);
		assertTrue(t1.equals(t2));
		assertTrue(t2.equals(t1));

		UserpType t3= createLargeTypeDAG(leaf3);
		assertFalse(t1.equals(t3));
		assertFalse(t3.equals(t1));
	}
}
