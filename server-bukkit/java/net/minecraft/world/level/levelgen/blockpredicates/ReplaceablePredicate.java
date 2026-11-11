// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.block.state.BlockState;
public class ReplaceablePredicate extends StateTestingPredicate {
    public static final MapCodec<ReplaceablePredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.ReplaceablePredicate.class, "CODEC");
    public ReplaceablePredicate(Vec3i offset) {
        super(offset);
    }
    protected boolean test(BlockState state) {
        return state.canBeReplaced();
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.REPLACEABLE;
    }
}
