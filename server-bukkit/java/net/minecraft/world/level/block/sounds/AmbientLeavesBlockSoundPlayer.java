// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.sounds;
import net.minecraft.core.Direction;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
public record AmbientLeavesBlockSoundPlayer(Optional<Holder<SoundEvent>> ambientSound, int chance, Optional<TagKey<Block>> satisfyingBlocks, int nearbySatisfyingBlocksRequired, int nearbySameLeavesRequired) {
    private static final AmbientLeavesBlockSoundPlayer kNoSound = new AmbientLeavesBlockSoundPlayer(Optional.empty(), 0, Optional.empty(), 0, 0);
    public static final Codec<AmbientLeavesBlockSoundPlayer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.sounds.AmbientLeavesBlockSoundPlayer.class, "CODEC");
    public static AmbientLeavesBlockSoundPlayer of(Holder<SoundEvent> ambientSound, TagKey<Block> satisfyingBlocks) {
        return new AmbientLeavesBlockSoundPlayer(Optional.of(ambientSound), 300, Optional.of(satisfyingBlocks), 1, 3);
    }
    public static AmbientLeavesBlockSoundPlayer noAmbientSound() {
        return kNoSound;
    }
    public void playAmbientLeavesSounds(Level level, BlockPos pos, Block block, RandomSource random) {
        if (ambientSound.isEmpty() || random.nextInt(chance) != 0) {
            return;
        }
        TagKey<Block> satisfying = satisfyingBlocks.orElse(null);
        int logs = 0;
        int leaves = 0;
        for (Direction direction : Direction.Plane.HORIZONTAL) {
            BlockState neighbour = level.getBlockState(pos.relative(direction));
            if (satisfying != null && neighbour.is(satisfying)) {
                logs++;
            }
            if (neighbour.is(block)) {
                leaves++;
            }
            if (logs == nearbySatisfyingBlocksRequired && leaves == nearbySameLeavesRequired) {
                level.playLocalSound((double) pos.getX(), (double) pos.getY(), (double) pos.getZ(), ambientSound.get().value(), SoundSource.AMBIENT, 1.0F, 1.0F, false);
                return;
            }
        }
    }
}
