// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.block.Block;
public class LongJumpToPreferredBlock<E extends Mob> extends LongJumpToRandomPos<E> {
    public LongJumpToPreferredBlock(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocity, Function<E, SoundEvent> getJumpSound, TagKey<Block> preferredBlockTag, float preferredBlocksChance, BiPredicate<E, BlockPos> acceptableLandingSpot) {
        super((net.minecraft.util.valueproviders.UniformInt) null, (int) 0, (int) 0, (float) 0, (java.util.function.Function) null);
    }
    protected void start(ServerLevel level, E body, long timestamp) {
        
    }
    protected Optional<LongJumpToRandomPos.PossibleJump> getJumpCandidate(ServerLevel level) {
        return Optional.empty();
    }
}
