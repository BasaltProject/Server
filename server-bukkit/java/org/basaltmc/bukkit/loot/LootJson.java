// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.loot;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.storage.loot.LootContext;

/** Writes facade loot objects in the file format the native loot service reads. */
public final class LootJson {
    public interface Node {
        void writeJson(StringBuilder out);
    }

    private LootJson() {
    }

    public static String of(Node node) {
        StringBuilder out = new StringBuilder();
        node.writeJson(out);
        return out.toString();
    }

    public static void value(StringBuilder out, Object value) {
        if (value instanceof Holder<?> holder) {
            Optional<?> key = holder.unwrapKey();
            if (key.isPresent() && key.get() instanceof net.minecraft.resources.ResourceKey<?> resource) {
                string(out, resource.identifier().toString());
                return;
            }
            value = holder.value();
        }
        if (value instanceof Node node) {
            node.writeJson(out);
        } else if (value == null) {
            out.append("null");
        } else {
            throw new IllegalStateException("No loot file form for " + value.getClass().getName());
        }
    }

    public static void string(StringBuilder out, String text) {
        out.append('"');
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == '"' || c == '\\') {
                out.append('\\');
            }
            out.append(c);
        }
        out.append('"');
    }

    public static void field(StringBuilder out, String name) {
        string(out, name);
        out.append(':');
    }

    public static void nodes(StringBuilder out, List<? extends Node> list) {
        out.append('[');
        for (int i = 0; i < list.size(); i++) {
            if (i > 0) {
                out.append(',');
            }
            list.get(i).writeJson(out);
        }
        out.append(']');
    }

    public static <T> void optional(StringBuilder out, String name, Optional<? extends Holder<T>> holder) {
        if (holder.isPresent()) {
            out.append(',');
            field(out, name);
            value(out, holder.get());
        }
    }

    public static double number(Node provider, boolean asFloat, LootContext context) {
        double value = org.basaltmc.bukkit.MiscNatives.lootNumber(of(provider), asFloat,
            context == null ? 0L : context.getRandom().nextLong());
        return Double.isNaN(value) ? 0.0 : value;
    }

    public static void holderSet(StringBuilder out, net.minecraft.core.HolderSet<?> set) {
        if (set.unwrapKey().isPresent()) {
            string(out, "#" + set.unwrapKey().get().location());
            return;
        }
        out.append('[');
        boolean first = true;
        for (Holder<?> each : set) {
            if (!first) {
                out.append(',');
            }
            first = false;
            string(out, each.getRegisteredName());
        }
        out.append(']');
    }

    public static void itemFilter(StringBuilder out, net.minecraft.advancements.predicates.ItemPredicate predicate) {
        var encoded = net.minecraft.advancements.predicates.ItemPredicate.CODEC
            .encodeStart(com.mojang.serialization.JsonOps.INSTANCE, predicate);
        out.append(encoded.getOrThrow(message -> new IllegalStateException("Cannot write item filter: " + message)));
    }
}
