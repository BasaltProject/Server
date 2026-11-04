// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.component;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;
import org.basaltmc.bukkit.facade.HolderRefs;

public record DebugStickState(Map<Holder<Block>, Property<?>> properties) {
    public static final DebugStickState EMPTY = new DebugStickState(Map.of());
    public static final Codec<DebugStickState> CODEC = Codec.unboundedMap(Identifier.CODEC, Codec.STRING).flatXmap(names -> {
        Map<Holder<Block>, Property<?>> out = new LinkedHashMap<>();
        for (Map.Entry<Identifier, String> entry : names.entrySet()) {
            Holder<Block> block = HolderRefs.of(Block.class, entry.getKey());
            Property<?> property = block.value() == null ? null : block.value().getStateDefinition().getProperty(entry.getValue());
            if (property == null) {
                return DataResult.error(() -> "No property " + entry.getValue() + " on " + entry.getKey());
            }
            out.put(block, property);
        }
        return DataResult.success(new DebugStickState(out));
    }, state -> {
        Map<Identifier, String> out = new LinkedHashMap<>();
        state.properties.forEach((block, property) -> out.put(block.unwrapKey().get().identifier(), property.getName()));
        return DataResult.success(out);
    });

    public DebugStickState withProperty(Holder<Block> block, Property<?> property) {
        Map<Holder<Block>, Property<?>> next = new HashMap<>(properties);
        next.put(block, property);
        return new DebugStickState(Map.copyOf(next));
    }
}
