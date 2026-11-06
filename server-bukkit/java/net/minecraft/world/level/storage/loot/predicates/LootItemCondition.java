// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextUser;
public interface LootItemCondition extends LootContextUser, Predicate<LootContext> {
    Codec<LootItemCondition> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.predicates.LootItemCondition.class, "DIRECT_CODEC");
    Codec<Holder<LootItemCondition>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.predicates.LootItemCondition.class, "CODEC");
    Codec<HolderSet<LootItemCondition>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.predicates.LootItemCondition.class, "LIST_CODEC");
    MapCodec<? extends LootItemCondition> codec();
    public interface Builder {
        LootItemCondition build();
        default Builder invert() {
            return InvertedLootItemCondition.invert(this);
        }
        default AnyOfCondition.Builder or(Builder other) {
            return AnyOfCondition.anyOf(this, other);
        }
        default AllOfCondition.Builder and(Builder other) {
            return AllOfCondition.allOf(this, other);
        }
    }
}
