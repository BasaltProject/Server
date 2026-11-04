// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.pathfinder;
public class BinaryHeap {
    private Node[] heap = new Node[128];
    private int size;
    public Node insert(Node node) {
        if (node.heapIdx >= 0) {
            throw new IllegalStateException("OW KNOWS!");
        }
        if (size == heap.length) {
            heap = java.util.Arrays.copyOf(heap, size << 1);
        }
        heap[size] = node;
        node.heapIdx = size;
        upHeap(size++);
        return node;
    }
    public void clear() {
        size = 0;
    }
    public Node peek() {
        return heap[0];
    }
    public Node pop() {
        Node top = heap[0];
        heap[0] = heap[--size];
        heap[size] = null;
        if (size > 0) {
            downHeap(0);
        }
        top.heapIdx = -1;
        return top;
    }
    public void remove(Node node) {
        heap[node.heapIdx] = heap[--size];
        heap[size] = null;
        if (size > node.heapIdx) {
            if (heap[node.heapIdx].f < node.f) {
                upHeap(node.heapIdx);
            } else {
                downHeap(node.heapIdx);
            }
        }
        node.heapIdx = -1;
    }
    public void changeCost(Node node, float newCost) {
        float old = node.f;
        node.f = newCost;
        if (newCost < old) {
            upHeap(node.heapIdx);
        } else {
            downHeap(node.heapIdx);
        }
    }
    public int size() {
        return size;
    }
    public boolean isEmpty() {
        return size == 0;
    }
    public Node[] getHeap() {
        return java.util.Arrays.copyOf(heap, size);
    }
    private void upHeap(int index) {
        Node node = heap[index];
        while (index > 0) {
            int parent = (index - 1) >> 1;
            Node up = heap[parent];
            if (node.f >= up.f) {
                break;
            }
            heap[index] = up;
            up.heapIdx = index;
            index = parent;
        }
        heap[index] = node;
        node.heapIdx = index;
    }
    private void downHeap(int index) {
        Node node = heap[index];
        while (true) {
            int left = 1 + (index << 1);
            int right = left + 1;
            if (left >= size) {
                break;
            }
            Node leftNode = heap[left];
            Node rightNode = right >= size ? null : heap[right];
            Node child = rightNode == null || leftNode.f < rightNode.f ? leftNode : rightNode;
            int childIndex = child == leftNode ? left : right;
            if (child.f >= node.f) {
                break;
            }
            heap[index] = child;
            child.heapIdx = index;
            index = childIndex;
        }
        heap[index] = node;
        node.heapIdx = index;
    }
}
