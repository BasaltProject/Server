// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelAccessor;
public class NotPredicate implements BlockPredicate {
    public static final MapCodec<NotPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.NotPredicate.class, "CODEC");
    private final BlockPredicate predicate;
    public NotPredicate(BlockPredicate predicate) {
        this.predicate = predicate;
    }
    public boolean test(LevelAccessor level, BlockPos origin) {
        return !predicate.test(level, origin);
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.NOT;
    }
}
