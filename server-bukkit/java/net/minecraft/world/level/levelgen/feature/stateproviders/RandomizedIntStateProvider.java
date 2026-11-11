// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
public class RandomizedIntStateProvider implements BlockStateProvider {
    private final Holder<BlockStateProvider> source;
    private final String propertyName;
    private net.minecraft.world.level.block.state.properties.IntegerProperty property;
    private final IntProvider values;
    private static net.minecraft.world.level.block.state.properties.IntegerProperty findProperty(BlockState state, String name) {
        for (net.minecraft.world.level.block.state.properties.Property<?> candidate : state.getProperties()) {
            if (candidate.getName().equals(name) && candidate instanceof net.minecraft.world.level.block.state.properties.IntegerProperty integer) {
                return integer;
            }
        }
        return null;
    }
    public static final MapCodec<RandomizedIntStateProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.RandomizedIntStateProvider.class, "CODEC");
    public RandomizedIntStateProvider(BlockStateProvider source, IntegerProperty property, IntProvider values) {
        this.source = Holder.direct(source);
        this.property = property;
        this.propertyName = property.getName();
        this.values = values;
        java.util.Collection<Integer> possible = property.getPossibleValues();
        for (int at = values.minInclusive(); at <= values.maxInclusive(); at++) {
            if (!possible.contains(at)) {
                throw new IllegalArgumentException("Property value out of range: " + property.getName() + ": " + at);
            }
        }
    }
    public RandomizedIntStateProvider(Holder<BlockStateProvider> source, String propertyName, IntProvider values) {
        this.source = source;
        this.propertyName = propertyName;
        this.values = values;
    }
    public MapCodec<RandomizedIntStateProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        BlockState base = source.value().getState(level, random, pos);
        if (property == null || !base.hasProperty(property)) {
            net.minecraft.world.level.block.state.properties.IntegerProperty found = findProperty(base, propertyName);
            if (found == null) {
                return base;
            }
            property = found;
        }
        return base.setValue(property, values.sample(random));
    }
}
