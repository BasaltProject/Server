// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Function;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.TypedDataComponent;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;

public record HashedPatchMap(Map<DataComponentType<?>, Integer> addedComponents, Set<DataComponentType<?>> removedComponents) {
    private static final int kMaxComponents = 256;
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static final StreamCodec<RegistryFriendlyByteBuf, DataComponentType<?>> TYPE_CODEC =
        (StreamCodec) ByteBufCodecs.registry(Registries.DATA_COMPONENT_TYPE);
    public static final StreamCodec<RegistryFriendlyByteBuf, HashedPatchMap> STREAM_CODEC = StreamCodec.of(
        (buf, patch) -> {
            buf.writeVarInt(patch.addedComponents().size());
            patch.addedComponents().forEach((type, hash) -> {
                TYPE_CODEC.encode(buf, type);
                buf.writeInt(hash);
            });
            buf.writeVarInt(patch.removedComponents().size());
            patch.removedComponents().forEach(type -> TYPE_CODEC.encode(buf, type));
        },
        buf -> {
            int added = buf.readVarInt();
            if (added > kMaxComponents) {
                throw new io.netty.handler.codec.DecoderException("Too many added components: " + added);
            }
            Map<DataComponentType<?>, Integer> addedComponents = new HashMap<>();
            for (int i = 0; i < added; i++) {
                addedComponents.put(TYPE_CODEC.decode(buf), buf.readInt());
            }
            int removed = buf.readVarInt();
            if (removed > kMaxComponents) {
                throw new io.netty.handler.codec.DecoderException("Too many removed components: " + removed);
            }
            Set<DataComponentType<?>> removedComponents = new HashSet<>();
            for (int i = 0; i < removed; i++) {
                removedComponents.add(TYPE_CODEC.decode(buf));
            }
            return new HashedPatchMap(addedComponents, removedComponents);
        });

    public static HashedPatchMap create(DataComponentPatch patch, HashGenerator hasher) {
        DataComponentPatch.SplitResult split = patch.split();
        Map<DataComponentType<?>, Integer> added = new IdentityHashMap<>(split.added().size());
        split.added().forEach(component -> added.put(component.type(), hasher.apply(component)));
        return new HashedPatchMap(added, split.removed());
    }

    public boolean matches(DataComponentPatch patch, HashGenerator hasher) {
        DataComponentPatch.SplitResult split = patch.split();
        if (!split.removed().equals(removedComponents) || addedComponents.size() != split.added().size()) {
            return false;
        }
        for (TypedDataComponent<?> component : split.added()) {
            Integer expected = addedComponents.get(component.type());
            if (expected == null || !hasher.apply(component).equals(expected)) {
                return false;
            }
        }
        return true;
    }

    @FunctionalInterface
    public interface HashGenerator extends Function<TypedDataComponent<?>, Integer> {}
}
