package com.silverdirk.userp;

import junit.framework.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: com.silverdirk.userp test suite</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class _Test_Package extends TestCase {
	public _Test_Package(String s) {
		super(s);
	}

	public static Test suite() {
		TestSuite suite = new TestSuite();
		suite.addTestSuite(com.silverdirk.userp._TestCommonTypes.class);
		suite.addTestSuite(com.silverdirk.userp._TestForwardOptimizedIndexedByteSequence.class);
		suite.addTestSuite(com.silverdirk.userp._TestRangeType.class);
		suite.addTestSuite(com.silverdirk.userp._TestSlidingByteSequence.class);
		suite.addTestSuite(com.silverdirk.userp._TestUnionType.class);
		suite.addTestSuite(com.silverdirk.userp._TestWholeType.class);
		suite.addTestSuite(com.silverdirk.userp._TestTupleType.class);
		return suite;
	}
}
