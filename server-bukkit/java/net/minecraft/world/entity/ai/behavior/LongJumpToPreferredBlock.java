// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.block.Block;

public class LongJumpToPreferredBlock<E extends Mob> extends LongJumpToRandomPos<E> {
    private final TagKey<Block> preferredBlockTag;
    private final float preferredBlocksChance;
    private final List<LongJumpToRandomPos.PossibleJump> notPreferredCandidates = new ArrayList<>();
    private boolean wantsPreferred;

    public LongJumpToPreferredBlock(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocity,
            Function<E, SoundEvent> getJumpSound, TagKey<Block> preferredBlockTag, float preferredBlocksChance, BiPredicate<E, BlockPos> acceptableLandingSpot) {
        super(timeBetweenLongJumps, maxLongJumpHeight, maxLongJumpWidth, maxJumpVelocity, getJumpSound, acceptableLandingSpot);
        this.preferredBlockTag = preferredBlockTag;
        this.preferredBlocksChance = preferredBlocksChance;
    }

    protected void start(ServerLevel level, E body, long timestamp) {
        super.start(level, body, timestamp);
        notPreferredCandidates.clear();
        wantsPreferred = body.getRandom().nextFloat() < preferredBlocksChance;
    }

    protected Optional<LongJumpToRandomPos.PossibleJump> getJumpCandidate(ServerLevel level) {
        if (!wantsPreferred) {
            return super.getJumpCandidate(level);
        }
        BlockPos.MutableBlockPos probe = new BlockPos.MutableBlockPos();
        while (!jumpCandidates.isEmpty()) {
            Optional<LongJumpToRandomPos.PossibleJump> candidate = super.getJumpCandidate(level);
            if (candidate.isPresent()) {
                if (level.getBlockState(probe.setWithOffset(candidate.get().targetPos(), Direction.DOWN)).is(preferredBlockTag)) {
                    return candidate;
                }
                notPreferredCandidates.add(candidate.get());
            }
        }
        return notPreferredCandidates.isEmpty() ? Optional.empty() : Optional.of(notPreferredCandidates.remove(0));
    }
}
