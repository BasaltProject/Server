// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import com.mojang.serialization.Codec;
import java.util.List;
import java.util.function.Consumer;
import net.minecraft.advancements.predicates.BlockPredicate;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.block.state.pattern.BlockInWorld;

public class AdventureModePredicate {
    public static final Codec<AdventureModePredicate> CODEC = Codec.withAlternative(BlockPredicate.CODEC.listOf(), BlockPredicate.CODEC, List::of)
        .xmap(AdventureModePredicate::new, predicate -> predicate.predicates);
    public static final StreamCodec<RegistryFriendlyByteBuf, AdventureModePredicate> STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistries(CODEC);
    public static final Component CAN_BREAK_HEADER = Component.translatable("item.canBreak").withStyle(net.minecraft.ChatFormatting.GRAY);
    public static final Component CAN_PLACE_HEADER = Component.translatable("item.canPlace").withStyle(net.minecraft.ChatFormatting.GRAY);

    private final List<BlockPredicate> predicates;

    public AdventureModePredicate(List<BlockPredicate> predicates) {
        this.predicates = predicates;
    }

    public boolean test(BlockInWorld blockInWorld) {
        for (BlockPredicate predicate : predicates) {
            if (predicate.matches(blockInWorld)) {
                return true;
            }
        }
        return false;
    }

    public void addToTooltip(Consumer<Component> consumer) {
        for (BlockPredicate predicate : predicates) {
            Component line = predicate.blocks().filter(set -> set.size() > 0).<Component>map(set -> {
                net.minecraft.network.chat.MutableComponent joined = Component.empty();
                for (int i = 0; i < set.size(); i++) {
                    if (i > 0) {
                        joined.append(Component.literal(", "));
                    }
                    joined.append(Component.literal(set.get(i).getRegisteredName()));
                }
                return joined.withStyle(net.minecraft.ChatFormatting.DARK_GRAY);
            }).orElse(Component.translatable("item.canUse.unknown").withStyle(net.minecraft.ChatFormatting.GRAY));
            consumer.accept(line);
        }
    }

    @Override
    public boolean equals(Object obj) {
        return obj instanceof AdventureModePredicate other && predicates.equals(other.predicates);
    }

    @Override
    public int hashCode() {
        return predicates.hashCode();
    }

    @Override
    public String toString() {
        return "AdventureModePredicate" + predicates;
    }
}
