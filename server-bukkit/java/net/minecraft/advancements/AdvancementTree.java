// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import net.minecraft.resources.Identifier;

public class AdvancementTree {
    private final Map<Identifier, AdvancementNode> nodes = new HashMap<>();
    private final Set<AdvancementNode> roots = new LinkedHashSet<>();
    private final Set<AdvancementNode> tasks = new LinkedHashSet<>();

    public void remove(Set<Identifier> ids) {
        for (Identifier id : ids) {
            AdvancementNode node = nodes.get(id);
            if (node == null) {
                continue;
            }
            remove(node);
        }
    }

    private void remove(AdvancementNode node) {
        for (AdvancementNode child : new ArrayList<>(childrenOf(node))) {
            remove(child);
        }
        nodes.remove(node.holder().id());
        roots.remove(node);
        tasks.remove(node);
    }

    private static List<AdvancementNode> childrenOf(AdvancementNode node) {
        List<AdvancementNode> out = new ArrayList<>();
        node.children().forEach(out::add);
        return out;
    }

    public void addAll(Iterable<AdvancementHolder> advancements) {
        List<AdvancementHolder> pending = new ArrayList<>();
        advancements.forEach(pending::add);
        boolean progress = true;
        while (!pending.isEmpty() && progress) {
            progress = false;
            List<AdvancementHolder> next = new ArrayList<>();
            for (AdvancementHolder holder : pending) {
                if (tryAdd(holder)) {
                    progress = true;
                } else {
                    next.add(holder);
                }
            }
            pending = next;
        }
    }

    private boolean tryAdd(AdvancementHolder holder) {
        if (nodes.containsKey(holder.id())) {
            return true;
        }
        AdvancementNode parent = null;
        if (holder.value().parent().isPresent()) {
            parent = nodes.get(holder.value().parent().get());
            if (parent == null) {
                return false;
            }
        }
        AdvancementNode node = new AdvancementNode(holder, parent);
        if (parent == null) {
            roots.add(node);
        } else {
            parent.addChild(node);
            tasks.add(node);
        }
        nodes.put(holder.id(), node);
        return true;
    }

    public void clear() {
        nodes.clear();
        roots.clear();
        tasks.clear();
    }

    public Iterable<AdvancementNode> roots() {
        return roots;
    }

    public Iterable<AdvancementNode> tasks() {
        return tasks;
    }

    public Collection<AdvancementNode> nodes() {
        return Set.copyOf(nodes.values());
    }

    public AdvancementNode get(Identifier id) {
        return nodes.get(id);
    }

    public AdvancementNode get(AdvancementHolder advancement) {
        return nodes.get(advancement.id());
    }

    public void repositionNodes() {
        for (AdvancementNode root : roots) {
            float[] row = {0.0F};
            layout(root, 0, row, new HashSet<>());
        }
    }

    private static void layout(AdvancementNode node, int depth, float[] row, Set<AdvancementNode> seen) {
        if (!seen.add(node)) {
            return;
        }
        node.setLocation(depth, row[0]);
        List<AdvancementNode> children = childrenOf(node);
        if (children.isEmpty()) {
            row[0] += 1.0F;
            return;
        }
        float first = row[0];
        for (AdvancementNode child : children) {
            layout(child, depth + 1, row, seen);
        }
        node.setLocation(depth, (first + row[0] - 1.0F) / 2.0F);
    }
}
