// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import java.util.HashSet;
import java.util.Set;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.level.block.Blocks;

class WeavingMobEffect extends MobEffect {
    private final ToIntFunction<RandomSource> maxCobwebs;

    protected WeavingMobEffect(MobEffectCategory category, int color, ToIntFunction<RandomSource> maxCobwebs) {
        super(category, color);
        this.maxCobwebs = maxCobwebs;
    }

    @Override
    public void onMobRemoved(ServerLevel level, LivingEntity mob, int amplifier, Entity.RemovalReason reason) {
        boolean allowed = mob instanceof Player || level.getGameRules().get(GameRules.MOB_GRIEFING);
        if (reason == Entity.RemovalReason.KILLED && allowed) {
            scatter(level, mob.getRandom(), mob.blockPosition());
        }
    }

    private void scatter(ServerLevel level, RandomSource random, BlockPos center) {
        int limit = maxCobwebs.applyAsInt(random);
        Set<BlockPos> chosen = new HashSet<>();
        for (BlockPos candidate : BlockPos.randomInCube(random, 15, center, 1)) {
            BlockPos support = candidate.below();
            boolean free = level.getBlockState(candidate).canBeReplaced();
            if (!chosen.contains(candidate) && free && level.getBlockState(support).isFaceSturdy(level, support, Direction.UP)) {
                chosen.add(candidate.immutable());
                if (chosen.size() >= limit) {
                    break;
                }
            }
        }
        for (BlockPos target : chosen) {
            level.setBlockAndUpdate(target, Blocks.COBWEB.defaultBlockState());
            level.levelEvent(3018, target, 0);
        }
    }
}
