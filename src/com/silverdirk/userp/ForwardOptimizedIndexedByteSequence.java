package com.silverdirk.userp;

import java.util.*;
import java.io.InputStream;
import java.io.IOException;
import java.io.EOFException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Forward-Optimized Indexed ByteSequence</p>
 * <p>Description: A tree-indexed ByteSequence optimized for forward-traversal</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * <p>This class is a combination of a singly-linked list and a Red/Black tree
 * that uses element indexes/sizes instead of keys.  The code is largely derived
 * from com.silverdirk.datastruct.SuperBST where I invented the indexing system
 * (independantly from all the people who invented it before me.  BAH)
 * which is largely derived from my previous rewrite of the Red/Black code from
 * the Red/Black demo at http://www.ececs.uc.edu/~franco/C321/html/RedBlack/
 * jointly written with Dr. John Franco of the University of Cincinnati, which
 * is a combination of his earlier red black demo and the deletion algorithm I
 * had previously written in C++, which was part of a project for Dr. Franco's
 * algorithms course which was done jointly with Tony Deeter.
 *
 * <p>As you might guess, I practically consider this code to be a family member
 * and bring it with me from project to project.  ;-)
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public class ForwardOptimizedIndexedByteSequence implements ByteSequence, Cloneable {
	/**
	 * This is the sentinel object, which is used instead of NULL as the
	 *   children of the leaves of the tree.
	 * The sentinel principle allows the tree algorithm to ignore the absence
	 *   of nodes, because this node will be there instead of NULL. This makes
	 *   the tree algorithms simpler.
	 * The sentinel has the nifty property of having itself as both its
	 *   left and right child, and it is its own parent.
	 * It also has a count of 0, since it isn't even a tree of count 1.
	 */
	public static final Node Sentinel= Node.createSentinel();

	/**
	 * RootSentinel is a node above the root of the tree, and serves a similar
	 * purpose as the leaf nodes.
	 * There is one root sentinel per tree.
	 * The left child of the root sentinel is the root node.
	 */
	protected Node rootSentinel;

	/**
	 * This value incrmeents each time a part of the byte sequence is altered.
	 * It is NOT changed when the sequence grows or is truncated.  The idea is
	 * that all byte streams remain valid for seek operations so long as the
	 * sequence is only growing to the right or being truncated from the left.
	 * If any alteration happens on the middle of the stream, stream seek
	 * operations will fail.
	 */
	protected int version;

	/** ForwardOptimizedIndexedByteSequence Constructor.
	 */
	public ForwardOptimizedIndexedByteSequence() {
		rootSentinel= Node.createSentinel();
		rootSentinel.parent= null; // this uniquely marks this as the root sentinel for this tree
		rootSentinel.left= Sentinel;
		version= 0;
	}

	public Object clone() {
		ForwardOptimizedIndexedByteSequence result= new ForwardOptimizedIndexedByteSequence();
		result.rootSentinel= (Node) rootSentinel.clone();
		return result;
	}

	public ByteSequence subSequence(long from, long to) {
		if (from > to)
			throw new IllegalArgumentException("Range must be from low-val to high-val");
		ByteSequence result= new ForwardOptimizedIndexedByteSequence();
		if (from == to)
			return result;
		NodeAndAddress fromLoc= getNodeFor(from);
		NodeAndAddress toLoc= getNodeFor(to-1);
		Node cur= fromLoc.node;
		Node stop= toLoc.node;
		short startIdx= (short)(fromLoc.node.bufferStart+(short)(from - fromLoc.address));
		if (cur == stop)
			result.append(cur.buffer, startIdx, (short)(startIdx+(short)(to-from)));
		else {
			result.append(cur.buffer, startIdx, fromLoc.node.bufferEnd);
			for (cur= cur.traverseToNext(); cur != stop; cur= cur.traverseToNext())
				result.append(cur.buffer, cur.bufferStart, cur.bufferEnd);
			short endIdx= (short)(toLoc.node.bufferStart+(short)(to - toLoc.address));
			result.append(cur.buffer, cur.bufferStart, endIdx);
		}
		return result;
	}

	/** Clear all nodes from the tree. */
	public void clear() {
		rootSentinel.left.detonate();
		rootSentinel.left= Sentinel;
	}

	/** Does the tree have zero bytes in it? */
	public boolean isEmpty() {
		return rootSentinel.left == Sentinel;
	}

	/** Get the number of bytes in the sequence. */
	public long getLength() {
		return rootSentinel.left.getLength();
	}

	public void append(byte[] buffer, short from, short to) {
		Node n= new Node();
		n.buffer= buffer;
		n.bufferStart= from;
		n.bufferEnd= to;
		insert(n, getLast(), false);
	}

	public void discard(long count) {
		Node cur= getFirst();
		while (count > 0) {
			int curLen= cur.bufferEnd-cur.bufferStart;
			if (curLen > count) {
				cur.alterBuffer(cur.buffer, (short)(cur.bufferStart+count), cur.bufferEnd);
				count= 0;
			}
			else {
				Node next= cur.next;
				cur.prune();
				cur= next;
				count-= curLen;
			}
		}
	}

	public IStream getStream(long fromIndex) {
		return new BlockIteratorIStream(fromIndex);
	}

	/** Get the node at the root of the tree. */
	Node getRoot() {
		return rootSentinel.left;
	}

	/** Get the left-most node in the tree.
	 */
	Node getFirst() {
		return rootSentinel.left.getLeftmostNode();
	}

	/** Get the right-most node in the tree.
	 */
	Node getLast() {
		return rootSentinel.left.getRightmostNode();
	}

	NodeAndAddress getNodeFor(long byteIndex) {
		return rootSentinel.left.getNodeFor((int)byteIndex);
	}

	void insert(Node newNode, Node parent, boolean before) {
		// If the tree is empty, then we add the node to the left subtree.
		if (isEmpty()) {
			parent= rootSentinel;
			before= true;
		}
		else {
			// now check to see if they gave us an internal node, and if so, get a leaf node.
			if ((before? parent.left : parent.right) != Sentinel) {
				parent= before? parent.traverseToPrev() : parent.next;
				before= !before;
			}
			// if some confused caller (or a broken tree) gave us a sentinel as a parent, throw something at them
			if (parent.isSentinel())
				throw new IllegalArgumentException("Cannot place new nodes under sentinel nodes.");
		}

		if (!newNode.isUnused())
			throw new IllegalArgumentException("Node already part of a tree.");
		newNode.code= 0;
		newNode.setColor(Node.Red);
		newNode.left= Sentinel;
		newNode.right= Sentinel;
		newNode.parent= parent;
		if (before) {
			parent.left= newNode;
			if (parent != rootSentinel)
				newNode.next= parent;
			Node prev= newNode.traverseToPrev();
			if (!prev.isSentinel())
				prev.next= newNode;
		}
		else {
			parent.right= newNode;
			newNode.next= parent.next;
			parent.next= newNode;
		}
		newNode.adjustLenCache(newNode.bufferEnd-newNode.bufferStart);

		balance(parent);
		rootSentinel.left.setColor(Node.Black);
	}

	/** Balance a tree that has just received a new child node.
	 * This function starts at the parent of the node just added and works its
	 * way to the top of the tree, rotating the tree as necessary to maintain
	 * the red/black properties.
	 *
	 * @param current The parent of the node just added.
	 */
	protected static final void balance(Node current) {
		// if Current is a black node, no rotations needed
		while (!current.isBlack()) {
			// Current is red, the imbalanced child is red, and parent is black.

			Node parent= current.parent;

			// if the Current is on the right of the parent, the parent is to the left
			if (parent.right == current) {
				// if the sibling is also red, we can pull down the color black from the parent
				if (parent.left.isRed()) {
					parent.left.setColor(Node.Black);
					current.setColor(Node.Black);
					parent.setColor(Node.Red);
					// jump twice up the tree. if Current reaches the HeadSentinel (black node), the loop will stop
					current= parent.parent;
				}
				else {
					// if the imbalance (red node) is on the left, and the parent is on the left,
					//  a "prep-slide" is needed. (see diagram)
					if (current.left.isRed())
						current.rotateRight();

					// Now we can do our left rotation to balance the tree.
					parent.rotateLeft();
					parent.setColor(Node.Red);
					parent.parent.setColor(Node.Black);
					return;
				}
			}
			// else the parent is to the right
			else if (parent.left == current) {
				// if the sibling is also red, we can pull down the color black from the parent
				if (parent.right.isRed()) {
					parent.right.setColor(Node.Black);
					current.setColor(Node.Black);
					parent.setColor(Node.Red);
					// jump twice up the tree. if Current reaches the HeadSentinel (black node), the loop will stop
					current= parent.parent;
				}
				else {
					// if the imbalance (red node) is on the right, and the parent is on the right,
					//  a "prep-slide" is needed. (see diagram)
					if (current.right.isRed())
						current.rotateLeft();

					// Now we can do our right rotation to balance the tree.
					parent.rotateRight();
					parent.setColor(Node.Red);
					parent.parent.setColor(Node.Black);
					return;
				}
			}
		}
	}

	static class NodeAndAddress {
		long address;
		Node node;
		public NodeAndAddress(long address, Node node) {
			this.address= address;
			this.node= node;
		}
	}

	/** Class Node is the node object used by SuperBST.
	 * <p>Foremost, Node is a binary search tree node.  It has pointers
	 * to the left and right child nodes, and a pointer to the parent node.
	 *
	 * <p>Secondly, it implements the Red/Black balancing algorithm, where the balanced
	 * nodes are marked with the color black and the unbalanced nodes are marked red.
	 * It keeps track of the color with an integer and symbolic constants.
	 *
	 * <p>For the forward-optimization, I added "Next" pointers.  It doesn't
	 * provide much speedup over traversing the parent/left pointers, but the
	 * "next" value will never change so long as the tree is only appended on
	 * the right and truncated on the left.  This is the common case, for
	 * ByteSequence.
	 *
	 * <p>One pecularity of this implementation is that instead of using null
	 * pointers to indicate the edges of the tree I used "sentinel" nodes, which
	 * is actually one static global node object.  This means that a leaf of the
	 * tree will have left and right pointers both pointing to the Sentinel node.
	 * The parent pointer never points to Sentinel, however; each tree
	 * allocates a special "root sentinel" instead.  The sentinel concept simplifies
	 * the balancing and deletion algorithms by reducing the number of checks for
	 * null pointers.  The sentinel node's left and right children are itself.
	 * No guarantees are made about where the sentinel's parent pointer points, but
	 * the algorithm never uses that pointer anyway.
	 */
	static class Node implements Cloneable {
		public static final int
			ColorMask= 0x80000000,
			SizeMask= 0x7FFFFFFF,
			Red= 0x80000000,
			Black= 0;

		Node parent;
		Node left;
		Node right;
		Node next;
		int code;
		short bufferStart;
		short bufferEnd;
		byte[] buffer;

		/** Super Binary Search Tree Node constructor.
		 * This initializes the pointers of the node and assigns its object
		 * for use in node comparisons.
		 * @param obj The object containing the key for sorting this node.
		 */
		protected Node() {
			parent= null;
			left= Sentinel;
			right= Sentinel;
			next= null;
		}

		static final Node createSentinel() {
			Node result= new Node();
			result.left= result;
			result.right= result;
			result.parent= result;
			result.code= 0;
			result.bufferStart= result.bufferEnd= 0;
			return result;
		}

		protected boolean isUnused() {
			return next == null;
		}

		/** Is this node the global Sentinel or a RootSentinel?
		 */
		public final boolean isSentinel() {
			return right == this;
		}

		/** Clone this node.
		 * This function must be overridden with code that duplicates the
		 * descendant class.  Node fields can be cloned by calling the
		 * 'cloneNodeFields' protected function.
		 */
		public Object clone() {
			if (this == Sentinel)
				return Sentinel;
			Node result= new Node();
			result.code= code;
			result.buffer= buffer;
			result.bufferStart= bufferStart;
			result.bufferEnd= bufferEnd;
			result.parent= null;
			result.right= (Node) right.clone();
			result.left= (Node) left.clone();
			result.left.getRightmostNode().next= result;
			result.next= result.right.getLeftmostNode();
			return result;
		}

		/** "Detonate" this node, destroying its portion of the tree.
		 * This is called by the root when the tree should be destroyed without
		 * taking the time to prune each node.  In a garbage-collected world,
		 * this has no real purpose, but could theoretically speed up garbage
		 * collection.  It would also serve a purpose if I planned to re-use
		 * node objects, which I don't for this class.
		 */
		private final void detonate() {
			parent= null;
			next= null;
			if (!left.isSentinel()) {
				left.detonate();
				left= Sentinel;
			}
			if (!right.isSentinel()) {
				right.detonate();
				right= Sentinel;
			}
			code= 0;
		}

		/** Get the parent node of this node.
		 * This value might be a root sentinel node; check the isSentinel property
		 * of the return value.
		 */
		public final Node getParent() {
			return parent;
		}

		/** Get the left child of this node.
		 * This value might be the global Sentinel object.
		 */
		public final Node getLeft() {
			return left;
		}

		/** Get the right child of this node.
		 * This value might be the global Sentinel object.
		 */
		public final Node getRight() {
			return right;
		}

		/** Get the color (Red or Black) of this node. */
		public final int getColor() {
			return code & ColorMask;
		}

		protected void setColor(int color) {
			code= (code & ~ColorMask) | color;
		}

		public final boolean isBlack() {
			return (code & ColorMask) == Black;
		}

		public final boolean isRed() {
			return (code & ColorMask) == Red;
		}

		/** Get the number of nodes in the tree formed by this node.
		 * This value includes all nodes to the left, right, center, and this node
		 * itself.
		 */
		public final int getLength() {
			return code & SizeMask;
		}

		protected final void recalcLength() {
			code= (code & ~SizeMask) | (left.getLength() + right.getLength() + (bufferEnd-bufferStart));
		}

		protected final void adjustLenCache(int ofs) {
			for (Node temp= this; temp.parent != null; temp= temp.parent)
				temp.code+= ofs; // this is safe, since the count is the low-order bits, and can't go negative
		}

		final void alterBuffer(byte[] buffer, short startPos, short stopPos) {
			int prevLen= bufferEnd-bufferStart;
			this.buffer= buffer;
			bufferStart= startPos;
			bufferEnd= stopPos;
			int diff= (bufferEnd-bufferStart) - prevLen;
			if (diff != 0)
				adjustLenCache(diff);
		}

		/** Get the left-most node beneath this one in the tree. */
		final Node getLeftmostNode() {
			Node result= this;
			while (result.left != Sentinel)
				result= result.left;
			return result;
		}

		/** Get the right-most node beneath this one in the tree. */
		final Node getRightmostNode() {
			Node result= this;
			while (result.right != Sentinel)
				result= result.right;
			return result;
		}

		final NodeAndAddress getNodeFor(int byteIndex) {
			if (byteIndex >= getLength() || byteIndex < 0)
				throw new IndexOutOfBoundsException();
			int offset= byteIndex;
			Node current= this;
			while (true) {
				int leftLen= current.left.getLength();
				if (offset < leftLen)
					current= current.left;
				else {
					offset-= leftLen;
					int curLen= current.bufferEnd-current.bufferStart;
					if (offset < curLen)
						break;
					else {
						offset-= curLen;
						current= current.right;
					}
				}
			}
			return new NodeAndAddress(byteIndex-offset, current);
		}

		/** Get the index of the first byte of this node within its tree. */
		final int getStartIndex() {
			int leftCount= left.getLength();
			Node current= this;
			while (!current.parent.isSentinel()) {
				if (current.parent.right == current)
					leftCount+= current.parent.getLength() - current.getLength();
				current= current.parent;
			}
			return leftCount;
		}

		/** Get the previous noce in the tree in left-to-right order. */
		Node traverseToPrev() {
			// If we are not at a leaf, move to the right-most node
			//  in the tree to the left of this node.
			if (left != Sentinel)
				return left.getRightmostNode();

			// Else walk up the tree until we see a parent node to the left
			else {
				Node node= this;
				Node nodeParent= parent;
				while (nodeParent.left == node && !nodeParent.isSentinel()) {
					node= nodeParent;
					nodeParent= node.parent;
				}
				return nodeParent.isSentinel()? Sentinel : nodeParent;
			}
		}

		/** Get the next node in the tree in left-to-right order. */
		Node traverseToNext() {
			// If we are not at a leaf, move to the left-most node
			//  in the tree to the right of this node.
			if (right != Sentinel)
				return right.getLeftmostNode();

			// Else walk up the tree until we see a parent node to the right
			else {
				Node node = this;
				Node nodeParent = parent;
				while (nodeParent.right == node && !nodeParent.isSentinel()) {
					node = nodeParent;
					nodeParent = node.parent;
				}
				return nodeParent.isSentinel()? Sentinel : nodeParent;
			}
		}

		// Rotate the current node rightward, and move the left child up to take this one's place.
		protected final void rotateRight() {
			Node replacement= left;

			if (parent.right == this) parent.right= replacement;
			else parent.left= replacement;
			replacement.parent= parent;

			left= replacement.right;
			replacement.right.parent= this;

			replacement.right= this;
			parent= replacement;

			recalcLength();
			parent.recalcLength();
		}

		// Rotate the current node leftward, and move the right child up to take this one's place.
		protected final void rotateLeft() {
			Node replacement= right;

			if (parent.right == this) parent.right= replacement;
			else parent.left= replacement;
			replacement.parent= parent;

			right= replacement.left;
			replacement.left.parent= this;

			replacement.left= this;
			parent= replacement;

			recalcLength();
			parent.recalcLength();
		}

		/** Prune this node from its tree.
		 * <p>This function eventually ends in a call to leafPrune, which does most of
		 * the work.  It first checks to see if the cureent node is a leaf, and if
		 * not it finds either the next or the previous node (which will be a leaf)
		 * and prunes it, then replaces this node with the pruned node.
		 *
		 * <p>Note that my definition of a leaf is any node with one or zero children.
		 * This is because a node with one child is trivially easy to remove, and
		 * is therefore more or less a leaf.
		 *
		 * <p>A tree manip exception is thrown if this node isn't in a tree or if
		 * it is a sentinel.
		 */
		final void prune() {
			if (isSentinel())
				throw new IllegalArgumentException("Cannot prune sentinel nodes!");
			if (isUnused())
				throw new IllegalArgumentException("Node cannot be pruned: not in any tree.");

			// If this is a leaf node (or almost a leaf) we can just prune it
			if (left == Sentinel || right == Sentinel)
				leafPrune();
			else {
				// Otherwise we need a successor.  We are guaranteed to have
				//   one because the current node has 2 children.
				// Use the prev node, because we need to fix up the linked list
				//   and we'll have to find the prev node anyway.
				Node successor= traverseToPrev();
				successor.leafPrune();

				// now exchange the successor for the current node
				successor.next= next;
				successor.right= right;
				right.parent= successor;
				successor.left= left;
				left.parent= successor;
				successor.parent= parent;
				if (parent.left == this) parent.left= successor;
				else if (parent.right == this) parent.right= successor;
				successor.setColor(getColor());

				// leafPrune sets the length to 0, so we need to restore it
				successor.recalcLength();
				// We will also need to propogate the length difference upward
				parent.adjustLenCache(successor.getLength() - getLength());
			}
			left= right= Sentinel;
			parent= null;
			code= 0;
		}

		/** Prune a node with one or zero children from its tree.
		 * This function is private, but deserves a doc comment anyway since it
		 * is the heart of the Red/Black deletion algorithm.
		 * <p>Specific documentation of the logic involved can be seen in the code,
		 * but here's an overview:
		 * <p>First, I make quite a few assumptions based on the definition of a
		 * correct red / black binary search tree.  There is no recovery logic; if
		 * the tree is corrupted this function will likely make things worse.  The
		 * tree should never get corrupted anyway, with the access permissions on
		 * these classes.  For reference, these rules are:
		 * <ul>
		 * <li>A red node cannot have any red children or a red parent.</li>
		 * <li>There must always be the same number of black nodes on any path down
		 * the tree.  By extension: if a node has only one child that child must be
		 * red, and a black node will always have a sibling.</li>
		 * </ul>
		 * The algorithm runs as follows:
		 * <p>First see if the node is a red leaf or has only one child. Remove
		 * these the obvious way. (and no balancing required)<br>
		 * else, Remove the node anyway, creating a black-node defecit.<br>
		 * While the imbalance has not been fixed, either try to change sibling
		 * black nodes to red, or rotate sibling nodes to our side of the tree.
		 * Sometimes rotating siblings to our side lets us set nodes on our side to
		 * black and balances the tree.<br>
		 * When a red parent node is found, change it to black, balancing the tree.
		 */
		private void leafPrune() {
			boolean LeftSide = (parent.left == this);

			// First, reduce the count from here on up to the root sentinel.
			adjustLenCache(-(bufferEnd-bufferStart));

			// if the node is red and has at most one child, then it has no child.
			// Prune it.
			if (isRed()) {
				if (LeftSide) parent.left= Sentinel;
				else parent.right= Sentinel;
			}
			// Node is black here.  If it has a child, the child will be red.
			else if (left != Sentinel) {
				// swap with child
				left.setColor(Black);
				left.parent= parent;
				if (LeftSide) parent.left= left;
				else parent.right= left;
				return;
			}
			// Node is black here.  If it has a child, the child will be red.
			else if (right != Sentinel) {
				// swap with child
				right.setColor(Black);
				right.parent= parent;
				if (LeftSide) parent.left= right;
				else parent.right= right;
				return;
			}
			else {
				/*
				 Now, we have determined that Node is a black leaf node with no children.
				 The tree must have the same number of black nodes along any path from root
				 to leaf.  We want to remove a black node, disrupting the number of black
				 nodes along the path from the root to the current leaf.  To correct this,
				 we must either shorten all other paths, or add a black node to the current
				 path.  Then we can freely remove our black leaf.
				 While we are pointing to it, we will go ahead and delete the leaf and
				 replace it with the sentinel (which is also black, so it won't affect
				 the algorithm).
				 */
				if (LeftSide)
					parent.left= Sentinel;
				else
					parent.right= Sentinel;
				Node sibling= (LeftSide)? parent.right : parent.left;
				Node current= this;
				Node curParent= parent;

				// Loop until the current node is red, or until we get to the root node.
				// (The root node's parent is the RootSentinel.)
				while (current.isBlack() && !curParent.isSentinel()) {
					// If the sibling is red, we are unable to reduce the number of black
					//  nodes in the sibling tree, and we can't increase the number of black
					//  nodes in our tree..  Thus we must do a rotation from the sibling
					//  tree to our tree to give us some extra (red) nodes to play with.
					// This is Case 1 from the text
					if (sibling.isRed()) {
						curParent.setColor(Red);
						sibling.setColor(Black);
						if (LeftSide) {
							curParent.rotateLeft();
							sibling= curParent.right;
						}
						else {
							curParent.rotateRight();
							sibling= curParent.left;
						}
						continue;
					}
					// Sibling will be black here

					// If the sibling is black and both children are black, we have to
					//  reduce the black node count in the sibling's tree to match ours.
					// This is Case 2a from the text.
					if (sibling.right.isBlack() && sibling.left.isBlack()) {
						sibling.setColor(Red);
						// Now we move one level up the tree to continue fixing the
						// other branches.
						current= curParent;
						curParent= current.parent;
						LeftSide= (curParent.left == current);
						sibling= (LeftSide)? curParent.right : curParent.left;
						continue;
					}
					// Sibling will be black with 1 or 2 red children here

					// << Case 2b is handled by the while loop. >>

					// If one of the sibling's children are red, we again can't make the
					//  sibling red to balance the tree at the parent, so we have to do a
					//  rotation.  If the "near" nephew is red and the "far" nephew is
					//  black, we need to do a "prep-slide" (aka "double rotation")
					// After doing a rotation and rearranging a few colors, the effect is
					//  that we maintain the same number of black nodes per path on the far
					//  side of the parent, and we gain a black node on the current side,
					//  so we are done.
					// This is Case 4 from the text. (Case 3 is the double rotation)
					if (LeftSide) {
						if (sibling.right.isBlack()) { // Case 3 from the text
							sibling.rotateRight();
							sibling= curParent.right;
						}
						// now Case 4 from the text
						sibling.right.setColor(Black);
						sibling.setColor(curParent.getColor());
						curParent.setColor(Black);

						current= curParent;
						curParent= current.parent;
						current.rotateLeft();
						return;
					}
					else {
						if (sibling.left.isBlack()) { // Case 3 from the text
							sibling.rotateLeft();
							sibling = curParent.left;
						}
						// now Case 4 from the text
						sibling.left.setColor(Black);
						sibling.setColor(curParent.getColor());
						curParent.setColor(Black);

						current= curParent;
						curParent= current.parent;
						current.rotateRight();
						return;
					}
				}
				// Now, make the current node black (to fulfill Case 2b)
				// Case 4 will have exited directly out of the function.
				// If we stopped because we reached the top of the tree,
				//   the head is black anyway so don't worry about overwriting it.
				current.setColor(Black);
			}
		}
	}

	class BlockIteratorIStream extends IStream {
		long limitAddress;
		int origVersion;
		Node curBlock;

		long markAddress;

		int localPos;
		int localLimit;

		public BlockIteratorIStream(long address) {
			markAddress= -1;
			origVersion= version;
			try {
				seek(address);
			}
			catch (IOException ex) {
				throw new Error("seeking should never thow an exception during the constructor", ex);
			}
		}

		protected boolean nextBlock() throws IOException {
			if (curBlock.next == null)
				return false;
			nextBlock(curBlock.next, (short)0);
			return true;
		}

		protected void nextBlock(Node next, short offset) {
			curBlock= next;
			localPos= curBlock.bufferStart+offset;
			localLimit= curBlock.bufferEnd;
			limitAddress+= localLimit-localPos;
		}

		public long getSequencePos() {
			return limitAddress - (localLimit - localPos);
		}
		public int available() {
			return (int) Math.min(Integer.MAX_VALUE, getLength() - getSequencePos());
		}
		public boolean markSupported() {
			return true;
		}
		public void mark(int readlimit) {
			markAddress= getSequencePos();
		}
		public void reset() throws IOException {
			if (markAddress == -1)
				throw new IllegalStateException("Must call 'mark' before 'reset'");
			if (origVersion != version)
				throw new ConcurrentModificationException("Sequence has been modified: cannot seek.");
			seek(markAddress);
		}

		public int read() throws IOException {
			if (localPos == localLimit)
				if (!nextBlock())
					return -1;
			return curBlock.buffer[localPos++] & 0xFF;
		}

		public int read(byte[] buffer, int offset, int length) throws IOException {
			if (localPos == localLimit)
				if (!nextBlock())
					return -1;
			int maxSegment= Math.min(length, localLimit-localPos);
			System.arraycopy(curBlock.buffer, localPos, buffer, offset, maxSegment);
			localPos+= maxSegment;
			return maxSegment;
		}

		public byte forceRead() throws IOException {
			if (localPos == localLimit)
				if (!nextBlock())
					throw new EOFException();
			return curBlock.buffer[localPos++];
		}

		public void forceRead(byte[] buffer, int offset, int length) throws IOException {
			int bufferLimit= offset+length;
			while (offset < bufferLimit) {
				if (localPos == localLimit)
					if (!nextBlock())
						throw new EOFException();
				int maxSegment= Math.min(bufferLimit-offset, localLimit-localPos);
				System.arraycopy(curBlock.buffer, localPos, buffer, offset, maxSegment);
				offset+= maxSegment;
				localPos+= maxSegment;
			}
		}

		public long skip(long n) throws IOException {
			long startAddr= getSequencePos();
			long targetAddr= limitAddress + n;
			boolean success= true;
			while (limitAddress < targetAddr && success)
				success= nextBlock();
			if (success) {
				localPos= localLimit - (int)(limitAddress - targetAddr);
				return n;
			}
			else
				return getSequencePos() - startAddr;
		}

		public void forceSkip(long n) throws IOException {
			long targetAddr= getSequencePos() + n;
			while (limitAddress < targetAddr)
				if (!nextBlock())
					throw new EOFException();
			localPos= localLimit - (int)(limitAddress - targetAddr);
		}

		public void seek(long address) throws IOException {
			NodeAndAddress next= getNodeFor(address);
			limitAddress= next.address;
			long offset= address - limitAddress;
			if (offset > Short.MAX_VALUE)
				throw new Error("Bug in 'seek'");
			nextBlock(next.node, (short) offset);
		}
	}
}
