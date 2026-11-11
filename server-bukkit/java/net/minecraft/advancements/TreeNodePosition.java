// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;
import java.util.ArrayList;
import java.util.List;
public class TreeNodePosition {
    private final AdvancementNode node;
    private final TreeNodePosition parent;
    private final TreeNodePosition leftSibling;
    private final int order;
    private final List<TreeNodePosition> kids = new ArrayList<>();
    private TreeNodePosition ancestor = this;
    private TreeNodePosition thread;
    private int column;
    private float row = -1.0F;
    private float mod;
    private float change;
    private float shift;
    public TreeNodePosition(AdvancementNode node, TreeNodePosition parent, TreeNodePosition previousSibling, int childIndex, int depth) {
        if (node.advancement().display().isEmpty()) {
            throw new IllegalArgumentException("Can't position an invisible advancement!");
        }
        this.node = node;
        this.parent = parent;
        this.leftSibling = previousSibling;
        this.order = childIndex;
        this.column = depth;
        TreeNodePosition last = null;
        for (AdvancementNode child : node.children()) {
            last = adopt(child, last);
        }
    }
    private TreeNodePosition adopt(AdvancementNode child, TreeNodePosition last) {
        if (child.advancement().display().isEmpty()) {
            for (AdvancementNode grandchild : child.children()) {
                last = adopt(grandchild, last);
            }
            return last;
        }
        TreeNodePosition made = new TreeNodePosition(child, this, last, kids.size() + 1, column + 1);
        kids.add(made);
        return made;
    }
    private TreeNodePosition leftContour() {
        return thread != null ? thread : kids.isEmpty() ? null : kids.get(0);
    }
    private TreeNodePosition rightContour() {
        return thread != null ? thread : kids.isEmpty() ? null : kids.get(kids.size() - 1);
    }
    private void layoutRows() {
        if (kids.isEmpty()) {
            row = leftSibling == null ? 0.0F : leftSibling.row + 1.0F;
            return;
        }
        TreeNodePosition anchor = null;
        for (TreeNodePosition kid : kids) {
            kid.layoutRows();
            anchor = kid.separate(anchor == null ? kid : anchor);
        }
        applyShifts();
        float centre = (kids.get(0).row + kids.get(kids.size() - 1).row) / 2.0F;
        if (leftSibling == null) {
            row = centre;
        } else {
            row = leftSibling.row + 1.0F;
            mod = row - centre;
        }
    }
    private void applyShifts() {
        float pending = 0.0F;
        float drift = 0.0F;
        for (int at = kids.size() - 1; at >= 0; at--) {
            TreeNodePosition kid = kids.get(at);
            kid.row += pending;
            kid.mod += pending;
            drift += kid.change;
            pending += kid.shift + drift;
        }
    }
    private TreeNodePosition separate(TreeNodePosition defaultAncestor) {
        if (leftSibling == null) {
            return defaultAncestor;
        }
        TreeNodePosition innerRight = this;
        TreeNodePosition outerRight = this;
        TreeNodePosition innerLeft = leftSibling;
        TreeNodePosition outerLeft = parent.kids.get(0);
        float sumInnerRight = mod;
        float sumOuterRight = mod;
        float sumInnerLeft = innerLeft.mod;
        float sumOuterLeft = outerLeft.mod;
        while (innerLeft.rightContour() != null && innerRight.leftContour() != null) {
            innerLeft = innerLeft.rightContour();
            innerRight = innerRight.leftContour();
            outerLeft = outerLeft.leftContour();
            outerRight = outerRight.rightContour();
            outerRight.ancestor = this;
            float gap = innerLeft.row + sumInnerLeft - (innerRight.row + sumInnerRight) + 1.0F;
            if (gap > 0.0F) {
                innerLeft.ancestorFor(this, defaultAncestor).moveTo(this, gap);
                sumInnerRight += gap;
                sumOuterRight += gap;
            }
            sumInnerLeft += innerLeft.mod;
            sumInnerRight += innerRight.mod;
            sumOuterLeft += outerLeft.mod;
            sumOuterRight += outerRight.mod;
        }
        if (innerLeft.rightContour() != null && outerRight.rightContour() == null) {
            outerRight.thread = innerLeft.rightContour();
            outerRight.mod += sumInnerLeft - sumOuterRight;
            return defaultAncestor;
        }
        if (innerRight.leftContour() != null && outerLeft.leftContour() == null) {
            outerLeft.thread = innerRight.leftContour();
            outerLeft.mod += sumInnerRight - sumOuterLeft;
        }
        return this;
    }
    private void moveTo(TreeNodePosition right, float amount) {
        float gaps = right.order - order;
        if (gaps != 0.0F) {
            right.change -= amount / gaps;
            change += amount / gaps;
        }
        right.shift += amount;
        right.row += amount;
        right.mod += amount;
    }
    private TreeNodePosition ancestorFor(TreeNodePosition other, TreeNodePosition defaultAncestor) {
        return ancestor != null && other.parent.kids.contains(ancestor) ? ancestor : defaultAncestor;
    }
    private float accumulate(float modSum, int depth, float lowest) {
        row += modSum;
        column = depth;
        float low = Math.min(lowest, row);
        for (TreeNodePosition kid : kids) {
            low = kid.accumulate(modSum + mod, depth + 1, low);
        }
        return low;
    }
    private void lift(float offset) {
        row += offset;
        kids.forEach(kid -> kid.lift(offset));
    }
    private void publish() {
        node.setLocation((float) column, row);
        kids.forEach(TreeNodePosition::publish);
    }
    public static void run(AdvancementNode node) {
        if (node.advancement().display().isEmpty()) {
            throw new IllegalArgumentException("Can't position children of an invisible root!");
        }
        TreeNodePosition root = new TreeNodePosition(node, null, null, 1, 0);
        root.layoutRows();
        float lowest = root.accumulate(0.0F, 0, root.row);
        if (lowest < 0.0F) {
            root.lift(-lowest);
        }
        root.publish();
    }
}
