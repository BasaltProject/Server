// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.core.BlockPos;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.phys.Vec3;
public record ReplaceBlock(Vec3i offset, Optional<BlockPredicate> predicate, Holder<BlockStateProvider> blockState, Optional<Holder<GameEvent>> triggerGameEvent) implements EnchantmentEntityEffect {
    public static final MapCodec<ReplaceBlock> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ReplaceBlock.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        BlockPos at = BlockPos.containing(position).offset(offset);
        boolean allowed = predicate.isEmpty() || predicate.get().test(serverLevel, at);
        if (allowed && serverLevel.setBlockAndUpdate(at, blockState.value().getState(serverLevel, entity.getRandom(), at))) {
            triggerGameEvent.ifPresent(event -> serverLevel.gameEvent(entity, event, at));
        }


    }
    public MapCodec<ReplaceBlock> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
