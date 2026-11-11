// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;

import java.util.ArrayList;
import java.util.List;

public class AdvancementNode {
    private final AdvancementHolder holder;
    private final AdvancementNode parent;
    private final List<AdvancementNode> children = new ArrayList<>();
    private float x;
    private float y;

    public AdvancementNode(AdvancementHolder holder, AdvancementNode parent) {
        this.holder = holder;
        this.parent = parent;
    }

    public Advancement advancement() {
        return holder.value();
    }

    public AdvancementHolder holder() {
        return holder;
    }

    public boolean isTask() {
        return !isRoot();
    }

    public boolean isRoot() {
        return parent == null;
    }

    public AdvancementNode parent() {
        return parent;
    }

    public AdvancementNode root() {
        return getRoot(this);
    }

    public static AdvancementNode getRoot(AdvancementNode advancement) {
        AdvancementNode node = advancement;
        while (node.parent != null) {
            node = node.parent;
        }
        return node;
    }

    public Iterable<AdvancementNode> children() {
        return children;
    }

    public void addChild(AdvancementNode child) {
        children.add(child);
    }

    public void setLocation(float x, float y) {
        this.x = x;
        this.y = y;
    }

    public float x() {
        return x;
    }

    public float y() {
        return y;
    }

    @Override
    public boolean equals(Object obj) {
        return this == obj || obj instanceof AdvancementNode other && holder.equals(other.holder);
    }

    @Override
    public int hashCode() {
        return holder.hashCode();
    }

    @Override
    public String toString() {
        return holder.id().toString();
    }
}
