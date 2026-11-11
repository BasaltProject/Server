// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class WeatheringCopperChestBlock extends CopperChestBlock implements WeatheringCopper {
    public WeatheringCopperChestBlock(WeatheringCopper.WeatherState weatherState, SoundEvent openSound, SoundEvent closeSound, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.WeatheringCopper.WeatherState) null, (net.minecraft.sounds.SoundEvent) null, (net.minecraft.sounds.SoundEvent) null, (net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    public WeatheringCopper.WeatherState getAge() {
        return WeatheringCopper.ageOf(this);
    }
    public boolean isWaxed() {
        return false;
    }
}
