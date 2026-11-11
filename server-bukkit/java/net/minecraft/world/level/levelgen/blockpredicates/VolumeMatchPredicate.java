// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.LevelAccessor;
public record VolumeMatchPredicate(Vec3i min, Vec3i max, BlockPredicate match) implements BlockPredicate {
    public static final MapCodec<VolumeMatchPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.VolumeMatchPredicate.class, "CODEC");
    public BlockPredicateType<VolumeMatchPredicate> type() {
        return BlockPredicateType.VOLUME_MATCH;
    }
    public boolean test(LevelAccessor level, BlockPos pos) {
        for (BlockPos at : BlockPos.betweenClosed(pos.offset(min), pos.offset(max))) {
            if (!match.test(level, at)) {
                return false;
            }
        }
        return true;
    }
}
