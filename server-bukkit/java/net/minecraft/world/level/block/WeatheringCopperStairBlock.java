// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class WeatheringCopperStairBlock extends StairBlock implements WeatheringCopper {
    public WeatheringCopperStairBlock(WeatheringCopper.WeatherState weatherState, BlockState baseState, BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.randomTick(state, level, pos, random);
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return super.isRandomlyTicking(state);
    }
    public WeatheringCopper.WeatherState getAge() {
        return WeatheringCopper.ageOf(this);
    }
}
