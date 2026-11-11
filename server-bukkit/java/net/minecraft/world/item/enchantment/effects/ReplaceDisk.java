// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.phys.Vec3;
public record ReplaceDisk(LevelBasedValue radius, LevelBasedValue height, Vec3i offset, Optional<BlockPredicate> predicate, Holder<BlockStateProvider> blockState, Optional<Holder<GameEvent>> triggerGameEvent) implements EnchantmentEntityEffect {
    public static final MapCodec<ReplaceDisk> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.ReplaceDisk.class, "CODEC");
    public void apply(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position) {
        BlockPos center = BlockPos.containing(position).offset(offset);
        RandomSource random = entity.getRandom();
        int reach = (int) radius.calculate(enchantmentLevel);
        int tall = (int) height.calculate(enchantmentLevel);
        for (BlockPos pos : BlockPos.betweenClosed(center.offset(-reach, 0, -reach), center.offset(reach, Math.min(tall - 1, 0), reach))) {
            if (pos.distToCenterSqr(position.x(), pos.getY() + 0.5, position.z()) < Mth.square(reach)
                && predicate.map(test -> test.test(serverLevel, pos)).orElse(true)
                && serverLevel.setBlockAndUpdate(pos, blockState.value().getState(serverLevel, random, pos))) {
                triggerGameEvent.ifPresent(event -> serverLevel.gameEvent(entity, event, pos));
            }
        }
    }
    public MapCodec<ReplaceDisk> codec() {
        return (com.mojang.serialization.MapCodec) CODEC;
    }
}
