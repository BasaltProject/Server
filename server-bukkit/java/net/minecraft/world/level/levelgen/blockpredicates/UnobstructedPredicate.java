// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.LevelAccessor;
public record UnobstructedPredicate(Vec3i offset) implements BlockPredicate {
    public static final MapCodec<UnobstructedPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.UnobstructedPredicate.class, "CODEC");
    public BlockPredicateType<?> type() {
        return BlockPredicateType.UNOBSTRUCTED;
    }
    public boolean test(LevelAccessor level, BlockPos pos) {
        BlockPos at = pos.offset(offset);
        return level.getBlockState(at).getCollisionShape(level, at).isEmpty();
    }
}
